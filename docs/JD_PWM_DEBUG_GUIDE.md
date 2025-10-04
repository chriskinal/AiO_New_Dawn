# JD PWM Encoder Debug Guide

## Debug Firmware

The debug version of firmware v1.0.19-beta includes serial output to help diagnose JD PWM encoder issues.

## Serial Monitor Setup

1. Connect to the Teensy via USB
2. Open a serial monitor at 115200 baud
3. You should see debug messages prefixed with `JD_PWM_`

## Debug Messages Explained

### Initialization Messages

```
JD_PWM_INIT: Mode ENABLED, threshold=20
```
- Confirms JD PWM mode is enabled at startup
- Shows the configured threshold value

```
JD_PWM_INIT: Mode DISABLED (using analog pressure mode)
```
- Indicates JD PWM is disabled, using regular pressure sensor mode

### Interrupt Activity

```
JD_PWM_ISR: 50 interrupts/sec, last duty=2500us
```
- Shows PWM signal is being received
- Displays interrupt frequency (should match encoder frequency)
- Shows the last measured duty cycle in microseconds

### Motion Detection

```
JD_PWM: duty=2600us, prev=2550us, motion=10.0, pressure=10.0
```
- Shows when motion is detected
- `duty` = current PWM duty cycle
- `prev` = previous duty cycle
- `motion` = calculated motion value (0-255)
- `pressure` = value being sent to AgOpenGPS

### Kickout Monitoring

```
JD_PWM_KICKOUT: enabled=1, pressure=15, threshold=20, isKeyaMotor=0
```
- Periodic status update (every 2 seconds)
- Shows if JD PWM is enabled
- Current pressure/motion value
- Configured threshold
- Motor type (should be 0 for non-Keya)

```
JD_PWM_CHECK: pressure=15, threshold=20, wouldTrigger=NO
```
- Shows kickout decision logic
- `wouldTrigger=YES` means motion exceeds threshold

```
JD_PWM_KICKOUT_TRIGGERED: motion=25 > threshold=20
JD_PWM_KICKOUT: *** KICKOUT ACTIVATED ***
```
- Indicates kickout has been triggered

## Troubleshooting Steps

### 1. No Interrupt Messages

If you don't see `JD_PWM_ISR` messages:
- Check wiring to Kickout-A pin
- Verify PWM signal with oscilloscope/multimeter
- Ensure JD PWM is enabled in web interface

### 2. Interrupts but No Motion

If you see interrupts but motion stays at 0:
- Check duty cycle values - should be 500-4500us range
- Look for "Invalid duty cycle" messages
- Try moving wheel more aggressively

### 3. Motion Detected but No Kickout

If motion values appear but kickout doesn't trigger:
- Check threshold setting vs motion values
- Verify AgOpenGPS has "Pressure Sensor" enabled
- Look for `wouldTrigger=YES` but no activation

### 4. False or Missing Values

```
JD_PWM: Large jump detected! duty=4800, prev=2500
```
- Indicates signal quality issues or encoder problems

```
JD_PWM: Invalid duty cycle: 5000us (valid range: 500-4500us)
```
- PWM signal outside expected range

## What to Report

When reporting issues, please provide:

1. Serial output during:
   - Startup (JD_PWM_INIT messages)
   - Normal operation (ISR count)
   - Wheel movement (motion values)
   - When kickout should occur

2. Configuration details:
   - Web interface settings (enabled, threshold)
   - AgOpenGPS settings (pressure sensor enabled?)
   - Motor type being used

3. Hardware details:
   - Encoder model/type
   - Wiring configuration
   - Any signal conditioning used

## Expected Behavior

With proper setup you should see:
1. `JD_PWM_INIT: Mode ENABLED` at startup
2. `JD_PWM_ISR` messages showing consistent interrupt frequency
3. Motion values increasing when wheel is turned
4. Kickout activating when motion exceeds threshold
5. AgOpenGPS disengaging autosteer

## Debug Commands

While serial monitor is open, you can press:
- `?` - Show main menu (if available)
- `e` - Toggle event logger
- Other commands depend on firmware configuration