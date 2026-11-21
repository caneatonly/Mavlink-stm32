-- Record position setpoints received from the STM32.
local mavlink_msgs = require("MAVLink/mavlink_msgs")
local message_id = 86
local crc_extra = 5
local source_sysid, source_compid = 1, 154
local target_sysid, target_compid = 1, 1

mavlink:init(8, 1)
mavlink:register_rx_msgid(message_id)

local function record(message)
    local h = mavlink_msgs.decode_header(message)
    if h.protocol_version ~= 2 or h.msgid ~= message_id or
       h.sysid ~= source_sysid or h.compid ~= source_compid or
       h.payload_length < 52 or h.payload_length > 53 then
        return
    end
    local crc_data = message:sub(4, 12 + h.payload_length) .. string.char(crc_extra)
    if mavlink_msgs.generateCRC(crc_data) ~= h.checksum then
        return
    end
    local lat, lon, altitude = string.unpack("<i4i4f", message, 17)
    local type_mask, sysid, compid = string.unpack("<I2I1I1", message, 61)
    -- MAVLink2 omits the trailing zero for MAV_FRAME_GLOBAL.
    local frame = h.payload_length == 53 and string.byte(message, 65) or 0
    if frame ~= 0 or type_mask ~= 0x0DF8 or sysid ~= target_sysid or compid ~= target_compid or
       math.abs(lat) > 900000000 or math.abs(lon) > 1800000000 or
       not (altitude >= -500 and altitude <= 10000) then
        return
    end
    logger:write("T086", "Lat,Lon,Alt", "iif", lat, lon, altitude)
    gcs:send_text(6, "Gimbal position setpoint received")
end

local function update()
    for _ = 1, 4 do
        local message = mavlink:receive_chan()
        if not message then break end
        record(message)
    end
    return update, 100
end

return update, 100
