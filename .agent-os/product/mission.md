# Product Mission

## Pitch

AiO New Dawn is a modular agricultural control system that helps farmers using AgOpenGPS achieve precision farming capabilities by providing automated steering, section control, and RTK GPS positioning for agricultural equipment.

## Users

### Primary Customers

- **Precision Agriculture Farmers**: Small to medium farming operations seeking cost-effective autosteer solutions for tractors, sprayers, and implements
- **Agricultural Equipment Integrators**: Companies building or retrofitting agricultural equipment with precision farming technology
- **DIY Agricultural Technology Enthusiasts**: Tech-savvy farmers and makers who want to build their own precision agriculture systems

### User Personas

**John Smith** (45-65 years old)
- **Role:** Farm Owner/Operator
- **Context:** Runs a 500-2000 acre grain operation with multiple tractors and implements
- **Pain Points:** High fuel costs from overlapping passes, operator fatigue during long field days, inconsistent application rates
- **Goals:** Reduce input costs, improve field efficiency, enable night operations, reduce operator stress

**Mike Johnson** (35-50 years old)
- **Role:** Agricultural Equipment Dealer/Integrator
- **Context:** Sells and services agricultural equipment, looking to add precision farming capabilities
- **Pain Points:** Expensive proprietary autosteer systems, limited customization options, complex installation requirements
- **Goals:** Offer competitive precision farming solutions, easy installation process, reliable customer support

**Dave Wilson** (30-45 years old)
- **Role:** Agricultural Technology Enthusiast/Engineer
- **Context:** Technically skilled farmer or engineer interested in open-source agricultural technology
- **Pain Points:** Lack of customizable, open agricultural control systems, limited integration options with existing equipment
- **Goals:** Build custom precision agriculture solutions, contribute to open-source agricultural technology, experiment with advanced features

## The Problem

### High Cost of Proprietary Precision Agriculture Systems

Commercial autosteer and precision agriculture systems cost $15,000-$30,000 per tractor, making precision farming financially inaccessible for many small to medium operations. These systems often require expensive dealer installation and ongoing subscription fees.

**Our Solution:** AiO New Dawn provides a complete precision agriculture solution for under $2,000 in hardware costs with open-source software and community support.

### Limited Customization and Integration Flexibility

Proprietary systems lock farmers into specific hardware configurations and prevent customization for unique farming operations or equipment setups. Integration with third-party sensors or implements is often impossible or requires expensive add-on modules.

**Our Solution:** Modular, open-source architecture allows complete customization of hardware configurations, sensor types, and implement integration to match specific farming needs.

### Complex Installation and Maintenance Requirements

Most precision agriculture systems require certified dealer installation and specialized diagnostic equipment for troubleshooting, creating dependencies on dealer networks and increasing downtime during critical farming seasons.

**Our Solution:** Clear documentation, web-based configuration, and standard electronic components enable farmer self-installation and maintenance with basic electrical skills.

### Lack of Real-Time Diagnostics and Monitoring

Traditional systems provide limited visibility into system performance, sensor health, and operational parameters, making troubleshooting difficult and preventing proactive maintenance.

**Our Solution:** Built-in web interface with real-time telemetry, comprehensive event logging, and WebSocket-based monitoring enable immediate problem identification and resolution.

## Differentiators

### Open-Source Hardware and Software Architecture

Unlike John Deere, Trimble, and other proprietary systems, AiO New Dawn provides complete hardware schematics, source code, and documentation under open-source licenses. This enables farmer ownership, customization, and community-driven improvements while eliminating vendor lock-in and subscription fees.

### Modular Design with Universal Compatibility

Unlike proprietary systems designed for specific tractor brands, AiO New Dawn's modular architecture supports any agricultural equipment through standardized interfaces. The system adapts to existing hydraulic steering, electric steering motors, or proportional valves without requiring equipment modifications.

### Real-Time Web Interface with Remote Monitoring

Unlike traditional systems requiring proprietary handheld displays, AiO New Dawn provides a comprehensive web interface accessible from any device. Real-time WebSocket telemetry enables remote monitoring and diagnostics from smartphones, tablets, or computers, improving troubleshooting and operational visibility.

## Key Features

### Core Features

- **Automated Steering Control:** Sub-inch accuracy GPS guidance with support for multiple motor types including PWM drivers, CAN-based motors, and hydraulic valves
- **Dual GPS/GNSS Support:** Automatic failover between GPS receivers with RTK correction support for centimeter-level accuracy
- **16-Section Control:** Precise implement control for sprayers, seeders, and spreaders with individual section on/off capability
- **Multi-Sensor Fusion:** Combines wheel angle sensors, quadrature encoders, and IMU data for optimal steering performance
- **Real-Time Web Configuration:** Complete system setup and monitoring through responsive web interface accessible from any device

### Navigation and Positioning Features

- **RTK GPS Integration:** Support for real-time kinematic corrections via network or radio for centimeter-level positioning accuracy
- **IMU/INS Support:** Multiple IMU options including BNO08x, TM171, and UM981 integrated navigation systems for heading and roll data
- **Speed and Distance Tracking:** Accurate ground speed measurement with configurable speed pulse output for rate controllers
- **Coordinate System Support:** UTM and local coordinate transformations for compatibility with various field mapping systems

### Hardware Compatibility Features

- **Universal Motor Support:** Works with Cytron MD30C, IBT-2, DRV8701 PWM drivers, Keya CAN motors, and Danfoss hydraulic valves
- **Flexible Sensor Inputs:** Supports analog and digital sensors including wheel angle sensors, work switches, pressure sensors, and current monitors
- **CAN Bus Communication:** Built-in FlexCAN support for implement communication and motor control with auto-detection capabilities
- **I2C Device Management:** Thread-safe I2C bus management for RGB LEDs, PWM controllers, and sensor expansion

### System Management Features

- **Event Logging System:** Comprehensive logging with Serial, UDP syslog, and WebSocket output for debugging and monitoring
- **OTA Firmware Updates:** Web-based firmware updates without requiring programming hardware or technical expertise
- **Hardware Resource Management:** Automatic pin ownership tracking and shared resource coordination to prevent hardware conflicts
- **Visual Status Indicators:** Four RGB status LEDs with color-coded system states and real-time event feedback