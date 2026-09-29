/**
 * json_builder.h - Minimal JSON string builder for FFI responses.
 *
 * 轻量级 JSON 字符串构建器，用于 FFI 响应序列化。
 *
 * No external dependencies. Thread-safe per-instance (not shared).
 *
 * v0.9:  Fixed float precision (platform-consistent), added array helpers.
 * v0.16: Fixed nested comma handling with bracket-aware separator.
 *
 * Usage:
 *   JsonBuilder jb;
 *   jb.begin_object();
 *   jb.field("port", "COM36");
 *   jb.field("baud", 115200);
 *   jb.field("armed", true);
 *   jb.end_object();
 *   std::string result = jb.str();
 */

#ifndef MGCORE_JSON_BUILDER_H
#define MGCORE_JSON_BUILDER_H

#include <string>
#include <sstream>
#include <cstdio>

namespace mg::core::telemetry {

class JsonBuilder {
public:
    void begin_object() { sep_before_bracket(); out_ << "{"; after_bracket_ = true; }
    void end_object()   { out_ << "}"; after_bracket_ = false; }
    void begin_array()  { sep_before_bracket(); out_ << "["; after_bracket_ = true; }
    void end_array()    { out_ << "]"; after_bracket_ = false; }

    void field(const char* key, const std::string& v) { sep(); q(key); out_ << ":\"" << esc(v) << "\""; }
    void field(const char* key, const char* v)         { sep(); q(key); out_ << ":\"" << esc(v ? v : "") << "\""; }
    void field(const char* key, int v)                 { sep(); q(key); out_ << ":" << v; }
    void field(const char* key, long v)                { sep(); q(key); out_ << ":" << v; }
    void field(const char* key, long long v)           { sep(); q(key); out_ << ":" << v; }
    void field(const char* key, unsigned v)            { sep(); q(key); out_ << ":" << v; }
    void field(const char* key, unsigned long v)       { sep(); q(key); out_ << ":" << v; }
    void field(const char* key, unsigned long long v)  { sep(); q(key); out_ << ":" << v; }
    void field(const char* key, bool v)                { sep(); q(key); out_ << ":" << (v ? "true" : "false"); }
    void field(const char* key, double v)              { sep(); q(key); out_ << ":" << format_float(v); }
    void field(const char* key, float v)               { sep(); q(key); out_ << ":" << format_float(static_cast<double>(v)); }

    void raw_field(const char* key, const std::string& raw) { sep(); q(key); out_ << ":" << raw; }
    void append_json(const std::string& raw) { sep(); out_ << raw; }

    /// Append a pre-built JsonBuilder's content as a nested element
    void append_object(const JsonBuilder& other) { sep(); out_ << other.str(); }

    std::string str() const { return out_.str(); }
    void reset() { out_.str(""); out_.clear(); after_bracket_ = false; }

private:
    /// Called before field/append_json. Outputs comma if needed.
    void sep() {
        if (after_bracket_) {
            after_bracket_ = false;  // first element after { or [, no comma
        } else {
            out_ << ",";             // subsequent elements get comma
        }
    }

    /// Called before begin_object/begin_array when nested inside another container.
    void sep_before_bracket() {
        if (after_bracket_) {
            after_bracket_ = false;
        } else if (out_.tellp() > 0) {
            // Check if we're inside a container (last char is not [ or {)
            std::string s = out_.str();
            if (!s.empty()) {
                char last = s.back();
                if (last != '[' && last != '{') {
                    out_ << ",";
                }
            }
        }
    }

    void q(const char* k) { out_ << "\"" << esc(k) << "\""; }

    static std::string format_float(double v) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.8g", v);
        return std::string(buf);
    }

    static std::string esc(const std::string& s) {
        std::string r;
        r.reserve(s.size() + 16);
        for (unsigned char c : s) {
            switch (c) {
                case '"':  r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (int)c);
                        r += buf;
                    } else {
                        r += (char)c;
                    }
                    break;
            }
        }
        return r;
    }

    std::ostringstream out_;
    bool after_bracket_ = false;
};

} // namespace mg::core::telemetry

#endif /* MGCORE_JSON_BUILDER_H */