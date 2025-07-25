/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include "fboss/agent/platforms/sai/SaiBcmPlatform.h"

namespace facebook::fboss {

class Jericho3Asic;

class SaiJanga800bicPlatform : public SaiBcmPlatform {
 public:
  SaiJanga800bicPlatform(
      std::unique_ptr<PlatformProductInfo> productInfo,
      folly::MacAddress localMac,
      const std::string& platformMappingStr);
  ~SaiJanga800bicPlatform() override;
  HwAsic* getAsic() const override;

  uint32_t numLanesPerCore() const override {
    return 8;
  }

  uint32_t numCellsAvailable() const override {
    return 130665;
  }

  bool isSerdesApiSupported() const override {
    return true;
  }

  void initLEDs() override {}

  std::vector<PortID> getAllPortsInGroup(PortID /*portID*/) const override {
    return {};
  }

  std::vector<FlexPortMode> getSupportedFlexPortModes() const override {
    return {};
  }

  bool supportInterfaceType() const override {
    return true;
  }

  std::optional<sai_port_interface_type_t> getInterfaceType(
      TransmitterTechnology /*transmitterTech*/,
      cfg::PortSpeed /*speed*/) const override {
    return std::nullopt;
  }
  std::vector<sai_system_port_config_t> getInternalSystemPortConfig()
      const override;

 private:
  void setupAsic(
      std::optional<int64_t> switchId,
      const cfg::SwitchInfo& switchInfo,
      std::optional<HwAsic::FabricNodeRole> fabricNodeRole) override;

  /**
   * @brief Retrieves CPU port core and port index mapping from BCM
   * configuration
   *
   * Parses the BCM configuration to extract CPU port assignments to specific
   * cores and port indices. This method looks for ucode_port configurations
   * matching the kCpuUcodePorts array and extracts the core and port index
   * information using regex pattern matching.
   *
   * @return std::map<uint32_t, std::pair<uint32_t, uint32_t>> Map where:
   *         - Key: CPU port ID (0-3)
   *         - Value: Pair of (core index, port index within core)
   */
  std::map<uint32_t, std::pair<uint32_t, uint32_t>> getCpuPortsCoreAndPortIdx()
      const;

  std::unique_ptr<Jericho3Asic> asic_;
};

} // namespace facebook::fboss
