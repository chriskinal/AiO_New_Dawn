  1. ConfigManager Additions

  // Add to ConfigManager.h
  enum GPS2Mode {
      GPS2_MODE_DUAL_GPS = 0,          // Normal dual GPS operation (default)
      GPS2_MODE_RS232_PASSTHROUGH = 1  // Forward to RS232 with decimal limiting
  };

  // New members
  GPS2Mode gps2Mode;               // Default: GPS2_MODE_DUAL_GPS
  uint8_t gps2DecimalPrecision;    // Default: 6 (only used in passthrough mode)

  // New methods
  GPS2Mode getGPS2Mode() const { return gps2Mode; }
  void setGPS2Mode(GPS2Mode mode) { gps2Mode = mode; }
  uint8_t getGPS2DecimalPrecision() const { return gps2DecimalPrecision; }
  void setGPS2DecimalPrecision(uint8_t precision) { gps2DecimalPrecision = constrain(precision, 4, 8); }

  2. GNSSProcessor Extensions

  // Add to GNSSProcessor.h
  private:
      // RS232 passthrough members
      char gps2PassthroughBuffer[256];
      int gps2PassthroughIndex;

      // RS232 passthrough methods
      void processGPS2Passthrough(uint8_t data);
      void limitNMEAPrecision(char* sentence);
      void forwardToRS232(const char* sentence);

  public:
      // Mode-aware entry point
      void processGPS2Data(uint8_t data);

  3. GNSSProcessor Implementation

  // New method in GNSSProcessor.cpp
  void GNSSProcessor::processGPS2Data(uint8_t data) {
      if (configManager->getGPS2Mode() == GPS2_MODE_DUAL_GPS) {
          processGPS2(data);  // Existing dual GPS processing
      } else {
          processGPS2Passthrough(data);  // New RS232 passthrough
      }
  }

  void GNSSProcessor::processGPS2Passthrough(uint8_t data) {
      // Build complete NMEA sentence
      if (data == '$') {
          gps2PassthroughIndex = 0;
      }

      if (gps2PassthroughIndex < sizeof(gps2PassthroughBuffer) - 1) {
          gps2PassthroughBuffer[gps2PassthroughIndex++] = data;

          if (data == '\n') {
              gps2PassthroughBuffer[gps2PassthroughIndex] = '\0';

              // Check sentence type efficiently (like existing NMEA parser)
              bool needsPrecisionLimit = false;
              if (gps2PassthroughIndex > 6 && gps2PassthroughBuffer[0] == '$') {
                  // Check the sentence ID characters directly
                  if ((gps2PassthroughBuffer[3] == 'G' && gps2PassthroughBuffer[4] == 'G' && gps2PassthroughBuffer[5] == 'A') ||  // GGA
                      (gps2PassthroughBuffer[3] == 'R' && gps2PassthroughBuffer[4] == 'M' && gps2PassthroughBuffer[5] == 'C') ||  // RMC  
                      (gps2PassthroughBuffer[3] == 'G' && gps2PassthroughBuffer[4] == 'L' && gps2PassthroughBuffer[5] == 'L')) {   // GLL
                      needsPrecisionLimit = true;
                  }
              }

              if (needsPrecisionLimit) {
                  limitNMEAPrecision(gps2PassthroughBuffer);
              }

              forwardToRS232(gps2PassthroughBuffer);
              gps2PassthroughIndex = 0;
          }
      }
  }

  void GNSSProcessor::limitNMEAPrecision(char* sentence) {
      // Parse sentence and rebuild with limited decimal precision
      // Use NMEAMessageBuilder to reconstruct with proper checksum
      // Limit lat/lon fields to configured decimal places
  }

  4. Main.cpp Simplification

  // Replace the GPS2 state machine in loop() with:
  if (SerialGPS2.available()) {
      gnssProcessor.processGPS2Data(SerialGPS2.read());
  }

  5. Web Interface Update

  Add to Device Settings page:
  <h2>GPS2 Configuration</h2>
  <div class='form-group'>
      <label>GPS2 Mode:</label>
      <select id='gps2Mode' onchange='toggleGPS2Options()'>
          <option value='0'>Dual GPS (Better Accuracy)</option>
          <option value='1'>RS232 Passthrough</option>
      </select>
  </div>

  <div class='form-group' id='gps2PrecisionGroup' style='display: none;'>
      <label>Decimal Precision: <span id='precisionValue'>6</span> digits</label>
      <input type='range' id='gps2Precision' min='4' max='8' value='6' 
             oninput='updatePrecisionValue(this.value)'>
      <div class='help-text'>
          Limits position data decimal places before forwarding to RS232
      </div>
  </div>

  6. SimpleWebManager Updates

  - Add GPS2 mode and precision to GET/POST handlers
  - Save to EEPROM with existing GPS config