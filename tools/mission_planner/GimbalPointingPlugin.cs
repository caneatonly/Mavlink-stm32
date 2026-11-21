using System;
using MissionPlanner;
using MissionPlanner.Plugin;

public sealed class GimbalPointingPlugin : Plugin
{
    private const byte FlightSystem = 1;
    private const byte FlightComponent = 1;
    private const byte GimbalComponent = 154;
    private const ushort MapRoiCommand = 201;  // Mission Planner 1.3.83 Point Camera Here
    private const int TaskIdMask = 0xFFFFFF;
    private static readonly TimeSpan AckWindow = TimeSpan.FromSeconds(9);
    private readonly object sync = new object();
    private MAVLinkInterface port;
    private bool waitingForAck;
    private bool noticePending;
    private DateTime sentAt;
    private long lastTaskNumber;

    public override string Name { get { return "TZ130TM Pointing"; } }
    public override string Version { get { return "1.0"; } }
    public override string Author { get { return "STM32 MAVLink Bridge"; } }

    public override bool Init()
    {
        loopratehz = 20;
        return true;
    }

    public override bool Loaded()
    {
        lock (sync) Attach(Host.comPort);
        return true;
    }

    public override bool Exit()
    {
        lock (sync) Attach(null);
        return true;
    }

    private void Attach(MAVLinkInterface next)
    {
        if (port == next) return;
        if (port != null)
        {
            port.OnPacketSent -= ObserveSent;
            port.OnPacketReceived -= ObserveReceived;
        }
        port = next;
        waitingForAck = false;
        noticePending = false;
        if (port != null)
        {
            port.OnPacketSent += ObserveSent;
            port.OnPacketReceived += ObserveReceived;
        }
    }

    private void ObserveSent(object sender, MAVLink.MAVLinkMessage message)
    {
        if (message.msgid != (uint)MAVLink.MAVLINK_MSG_ID.COMMAND_INT) return;
        var command = message.ToStructure<MAVLink.mavlink_command_int_t>();
        if (command.command != MapRoiCommand ||
            command.target_system != FlightSystem || command.target_component != FlightComponent ||
            command.frame != (byte)MAVLink.MAV_FRAME.GLOBAL_RELATIVE_ALT ||
            command.x == 0 || command.y == 0) return;
        lock (sync)
        {
            if (sender != port || port.BaseStream == null || !port.BaseStream.IsOpen) return;
            // Until an ACK arrives, retries remain part of the pending request.
            waitingForAck = true;
            sentAt = DateTime.UtcNow;
            noticePending = false;
        }
    }

    private void ObserveReceived(object sender, MAVLink.MAVLinkMessage message)
    {
        if (message.msgid != (uint)MAVLink.MAVLINK_MSG_ID.COMMAND_ACK ||
            message.sysid != FlightSystem || message.compid != FlightComponent) return;
        var ack = message.ToStructure<MAVLink.mavlink_command_ack_t>();
        if (ack.command != MapRoiCommand ||
            (ack.target_system != 0 && ack.target_system != MAVLinkInterface.gcssysid) ||
            (ack.target_component != 0 && ack.target_component != (byte)MAVLink.MAV_COMPONENT.MAV_COMP_ID_MISSIONPLANNER)) return;
        lock (sync)
        {
            if (sender != port || !waitingForAck) return;
            waitingForAck = false;
            noticePending = ack.result == (byte)MAVLink.MAV_RESULT.ACCEPTED &&
                            DateTime.UtcNow - sentAt <= AckWindow;
        }
    }

    public override bool Loop()
    {
        var current = Host.comPort;
        int id;
        lock (sync)
        {
            Attach(current);
            if (current == null || current.BaseStream == null || !current.BaseStream.IsOpen)
            {
                waitingForAck = false;
                noticePending = false;
                return true;
            }
            if (!noticePending) return true;
            noticePending = false;
            // Float COMMAND_LONG parameters store integers exactly through 24 bits.
            long timeNumber = DateTime.UtcNow.Ticks / (TimeSpan.TicksPerSecond / 10);
            lastTaskNumber = Math.Max(timeNumber, lastTaskNumber + 1);
            if ((lastTaskNumber & TaskIdMask) == 0) lastTaskNumber++;
            id = (int)(lastTaskNumber & TaskIdMask);
        }
        // Send on the plugin thread, outside Mission Planner's packet callbacks.
        current.doCommand(FlightSystem, GimbalComponent, MAVLink.MAV_CMD.USER_2,
                          id, 0, 0, 0, 0, 0, 0, false, null);
        return true;
    }
}
