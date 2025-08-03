// NewDawnCANPlugin.h
// Custom CAN plugin for AgIsoStack that uses New Dawn's existing CAN infrastructure
// This avoids duplicating FlexCAN_T4 instances and saves significant memory

#ifndef NEW_DAWN_CAN_PLUGIN_H
#define NEW_DAWN_CAN_PLUGIN_H

#include <memory>
#include <queue>
#include "can_hardware_plugin.hpp"
#include "CANGlobals.h"
#include "CANManager.h"

namespace isobus {

class NewDawnCANPlugin : public CANHardwarePlugin {
public:
    /// @brief Constructor for the New Dawn CAN plugin
    /// @param channel The CAN channel to use (0=CAN1, 1=CAN2, 2=CAN3)
    explicit NewDawnCANPlugin(std::uint8_t channel);
    
    /// @brief Destructor
    virtual ~NewDawnCANPlugin() = default;
    
    /// @brief Returns if the plugin is valid and ready to use
    /// @return true if the plugin is ready, false otherwise
    bool get_is_valid() const override;
    
    /// @brief Closes the CAN channel
    void close() override;
    
    /// @brief Opens the CAN channel
    void open() override;
    
    /// @brief Reads a CAN frame from the buffer
    /// @param[out] canFrame The frame that was read
    /// @return true if a frame was read, false if the buffer was empty
    bool read_frame(isobus::CANMessageFrame &canFrame) override;
    
    /// @brief Writes a CAN frame to the bus
    /// @param[in] canFrame The frame to write
    /// @return true if the frame was written, false otherwise
    bool write_frame(const isobus::CANMessageFrame &canFrame) override;
    
private:
    std::uint8_t selectedChannel; ///< The CAN channel index
    bool isOpen; ///< Tracks if the channel is open
    std::queue<isobus::CANMessageFrame> rxQueue; ///< Receive queue
    
    /// @brief Callback for when a CAN message is received
    void messageReceivedCallback(const CAN_message_t &msg);
    
    /// @brief Get the FlexCAN instance for the selected channel
    /// @return Pointer to the FlexCAN instance, or nullptr if invalid
    FlexCAN_T4_Base* getCANInstance() const;
};

} // namespace isobus

#endif // NEW_DAWN_CAN_PLUGIN_H