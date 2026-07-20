from pymavlink.generator.mavgen import mavgen, Opts

out_dir = r'C:/Users/23802/Documents/groundcontrol/mgroundcontrol/third_party/mavlink/include/mavlink/v2.0'
xml_files = [r'C:/Users/23802/Documents/groundcontrol/mgroundcontrol/third_party/mavlink/message_definitions/v1.0/common.xml']

opts = Opts(output=out_dir, wire_protocol='2.0', language='C', validate=False, strict_units=False)
mavgen(opts, xml_files)
print('Done generating MAVLink C headers')
