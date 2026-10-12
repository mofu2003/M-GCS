/**
 * geo_math.h - 任务相关的地理计算工具
 *
 * 原先这里是 `MissionItemFactory`（编译期生成 MISSION_ITEM_INT 的静态工厂），
 * 那些 builder 在本轮盘点中确认无任何调用方（测绘删除后连带失效），已删除；
 * 只剩任务估算用到的 Haversine 距离，改以自由函数提供。
 */
#pragma once

namespace mg::core::mission {

/// 两个经纬度点之间的大圆距离（Haversine），单位米。
double haversine_distance(double lat1_deg, double lon1_deg,
                          double lat2_deg, double lon2_deg);

} // namespace mg::core::mission
