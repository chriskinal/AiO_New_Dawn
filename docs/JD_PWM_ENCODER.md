# John Deere Autotrac PWM Encoder Support

## Overview

The AiO New Dawn firmware (v1.0.19-beta and later) includes support for John Deere Autotrac PWM encoders as a steering wheel motion detection (kickout) method. This feature allows tractors equipped with John Deere's PWM-based steering wheel encoders to detect manual steering input and disengage autosteer accordingly.

## How It Works

The JD PWM encoder outputs a PWM signal with a duty cycle that changes based on steering wheel rotation:
- PWM frequency: ~200 Hz (5ms period)
- Duty cycle range: 4% to 94% (90% total range)
- No center position - encoder rotates continuously
- The firmware measures duty cycle changes to detect steering wheel motion
- Motion is converted to a percentage (0-100%) and sent to AgOpenGPS as "pressure" data
- Wheel position is scaled from 4-94% to 0-99% for AgOpenGPS display
- AgOpenGPS's pressure set point determines when autosteer disengages

## Hardware Connection

### Pin Assignment
- Connect the JD PWM encoder signal to the **Kickout-D pin** (digital kickout input)
- This is pin 3 on the Teensy 4.1 (physical pin location depends on your PCB version)

### Wiring
1. **Signal Wire**: Connect the PWM output from the JD encoder to Kickout-D (pin 3)
2. **Power**: Provide appropriate power to the encoder per John Deere specifications
3. **Ground**: Ensure common ground between encoder and AiO board

**IMPORTANT**: The JD PWM encoder requires the digital Kickout-D pin because it needs interrupt capability for accurate PWM measurement. The analog Kickout-A pin (A12) cannot generate interrupts on Teensy 4.1. If your encoder outputs 5V, you may need a diode (1N4148) in series to drop the voltage, or ensure your encoder outputs 3.3V compatible signals.

## Configuration

### Enable JD PWM Mode

1. Access the AiO web interface at `http://192.168.5.126`
2. Navigate to **Device Settings**
3. Under **Turn Sensor Configuration**:
   - Check the box for **John Deere PWM Encoder Mode**
   - Adjust the **JD PWM Sensitivity** slider (1-10):
     - 1 = Least sensitive (requires more wheel movement)
     - 10 = Most sensitive (requires less wheel movement)
     - Default = 5 (medium sensitivity)
4. Click **Apply Changes**

### AgOpenGPS Configuration

**IMPORTANT**: AgOpenGPS must be configured to use "Pressure Sensor" kickout mode for the JD PWM encoder to work. This is because the JD PWM feature sends its motion detection data through the pressure sensor channel for compatibility.

In AgOpenGPS, configure the steer module settings:
1. Open the **Steer Configuration** window
2. In the **Turn Sensor** or **Kickout** section:
   - **Enable "Pressure Sensor"** (REQUIRED - even though you're using a PWM encoder)
   - Set the pressure set point value (this controls when kickout triggers)
     - Lower set point = easier to trigger kickout
     - Higher set point = harder to trigger kickout
     - Typical range: 20-80
3. Disable other kickout methods (Encoder, Current) unless you have those sensors on different pins

**Why Pressure Mode?** The JD PWM encoder reuses the pressure sensor data channel to maintain compatibility with AgOpenGPS without requiring software modifications. AgOpenGPS sees the motion values as "pressure" readings.

## Calibration

### Finding the Right Threshold

1. Enable JD PWM mode in the web interface
2. In the web interface, adjust the JD PWM Sensitivity slider (start at 5)
3. In AgOpenGPS, set the pressure set point to a starting value (e.g., 30)
4. Start autosteer and try turning the steering wheel manually
5. Adjust settings based on your needs:
   - If kickout is too sensitive overall: Decrease the JD PWM Sensitivity slider in web interface
   - If kickout requires too much wheel movement: Increase the JD PWM Sensitivity slider
   - Fine-tune the exact kickout point using the pressure set point in AgOpenGPS

### Testing Procedure

1. With the tractor stationary and engine running:
   - Enable autosteer
   - Gently turn the steering wheel
   - Verify that autosteer disengages
   
2. Test different steering speeds:
   - Slow, deliberate movements
   - Quick corrections
   - Ensure kickout works reliably in both cases

3. Field testing:
   - Test in actual field conditions
   - Verify that normal field bumps don't trigger false kickouts
   - Ensure manual override works when needed

## Troubleshooting

### Kickout Not Working
- Verify JD PWM mode is enabled in Device Settings
- Check wiring connections
- Ensure the PWM signal is reaching the Kickout-D pin (pin 3)
- In AgOpenGPS, lower the pressure set point value

### False Kickouts
- In AgOpenGPS, increase the pressure set point value
- Check for loose connections or signal interference
- Verify encoder is properly mounted and not vibrating

### Using with Pressure Sensor
- JD PWM encoder uses the digital Kickout-D pin (pin 3)
- Analog pressure sensors use the Kickout-A pin (A12)
- Both can be configured, but only one kickout method will be active based on the JD PWM mode setting
- The firmware automatically switches between them based on configuration

### AgOpenGPS Shows Wrong Sensor Type
- This is normal - AgOpenGPS will show "Pressure Sensor" even though you're using JD PWM
- The firmware sends JD PWM data through the pressure channel for compatibility
- This does not affect functionality

## Technical Details

### Signal Processing
- Rising edge interrupt captures start time and calculates period
- Falling edge interrupt calculates duty time
- Duty cycle percentage = (duty time / period) * 100
- Motion is calculated as the change in duty cycle between samples
- Noise floor: 0-5us variations (0.1% duty cycle) are filtered out
- Actual motion: 6-56us changes (0.12-1.12% duty cycle) are detected
- Motion scaled so 2% duty cycle change = full scale (255)
- Position display shows 4-94% duty cycle scaled to 0-99% for AgOpenGPS

### Performance
- PWM measurement resolution: 1 microsecond
- Input frequency: ~200 Hz
- Duty cycle range: 4% (full CCW) to 94% (full CW)
- Update rate: Synchronized with main control loop (100Hz)
- Minimal CPU overhead using hardware interrupts

### Important Notes
- Pin 3 requires INPUT_PULLUP mode due to circuit design
- The observed 4-94% range may vary between encoder models
- If your encoder has a different range, the firmware will auto-adapt

## Compatibility

- Firmware version: 1.0.19-beta or later
- Requires Teensy 4.1
- Compatible with all motor driver types (PWM, Danfoss, Keya)
- Uses separate digital pin from analog pressure sensors

## Future Enhancements

Potential future improvements:
- Automatic set point calibration
- Dual-mode support (switching between pressure and JD PWM)
- Enhanced filtering algorithms
- Data logging for set point optimization