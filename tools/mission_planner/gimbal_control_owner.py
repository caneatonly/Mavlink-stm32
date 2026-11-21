"""Run from the Python script tab of a connected Mission Planner."""
import clr

clr.AddReference("MAVLink")
clr.AddReference("System.Windows.Forms")

import MAVLink
from System import Byte, Enum, Single
from System.Windows.Forms import DialogResult, MessageBox, MessageBoxButtons, MessageBoxDefaultButton, MessageBoxIcon

choice = MessageBox.Show(
    "Yes: Give the vendor UI control and pause angle forwarding.\n"
    "No: Return control to the flight controller.\n"
    "Cancel: Keep the current mode.",
    "Gimbal control",
    MessageBoxButtons.YesNoCancel,
    MessageBoxIcon.Question,
    MessageBoxDefaultButton.Button1,
)
if choice in (DialogResult.Yes, DialogResult.No):
    ui_control = Single(1 if choice == DialogResult.Yes else 0)
    control_owner = Enum.ToObject(MAVLink.MAV_CMD, 31010)
    zero = Single(0)
    if MAV.doCommand(Byte(1), Byte(154), control_owner, ui_control, zero, zero,
                     zero, zero, zero, zero, False, None):
        print("Request sent. Check [MODE] on the STM32 log port.")
    else:
        print("Connect Mission Planner to the flight controller first.")
