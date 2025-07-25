/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/cli/fboss2/utils/CmdUtils.h"
#include <fboss/agent/if/gen-cpp2/ctrl_types.h>
#include "folly/Conv.h"

#include <folly/logging/LogConfig.h>

#include <re2/re2.h>
#include <chrono>
#include <string>

using namespace std::chrono;

using facebook::fboss::cfg::SwitchInfo;
using facebook::fboss::cfg::SwitchType;

using folly::ByteRange;
using folly::IPAddress;
using folly::IPAddressV6;

namespace facebook::fboss::utils {

/* Takes a list of "friendly" interface names and returns a list of portID
   integers.  This is an operation that is frequently needed so making this
   available as a helper function

   The only way to do this is to get all portInfo from Thrift, compare interface
   name in the lambda function, and if they match add the PortID to the output
   list
*/
std::vector<int32_t> getPortIDList(
    const std::vector<std::string>& ifList,
    std::map<int32_t, facebook::fboss::PortInfoThrift>& portEntries) {
  std::vector<int32_t> portIDList;

  if (ifList.size()) {
    for (auto interface : ifList) {
      auto it = std::find_if(
          portEntries.begin(),
          portEntries.end(),
          [&interface](const auto& port) {
            return port.second.get_name() == interface;
          });
      if (it != portEntries.end()) {
        portIDList.push_back(it->first);
      } else {
        throw std::runtime_error(fmt::format(
            "{} is not a valid interface name on this device", interface));
      }
    }
  }
  return portIDList;
}

std::string getAddrStr(network::thrift::BinaryAddress addr) {
  auto ip = *addr.addr();
  char ipBuff[INET6_ADDRSTRLEN];
  if (ip.size() == 16) {
    inet_ntop(
        AF_INET6,
        &((struct in_addr*)ip.c_str())->s_addr,
        ipBuff,
        INET6_ADDRSTRLEN);
  } else if (ip.size() == 4) {
    inet_ntop(
        AF_INET,
        &((struct in_addr*)ip.c_str())->s_addr,
        ipBuff,
        INET_ADDRSTRLEN);
  } else {
    return "invalid";
  }
  return std::string(ipBuff);
}

std::string getAdminDistanceStr(AdminDistance adminDistance) {
  switch (adminDistance) {
    case AdminDistance::DIRECTLY_CONNECTED:
      return "DIRECTLY_CONNECTED";
    case AdminDistance::STATIC_ROUTE:
      return "STATIC_ROUTE";
    case AdminDistance::OPENR:
      return "OPENR";
    case AdminDistance::EBGP:
      return "EBGP";
    case AdminDistance::IBGP:
      return "IBGP";
    case AdminDistance::MAX_ADMIN_DISTANCE:
      return "MAX_ADMIN_DISTANCE";
  }
  throw std::runtime_error(
      "Unsupported AdminDistance: " +
      std::to_string(static_cast<int>(adminDistance)));
}

const std::string removeFbDomains(const std::string& host) {
  std::string hostCopy = host;
  const RE2 fbDomains(".facebook.com$|.tfbnw.net$");
  RE2::Replace(&hostCopy, fbDomains, "");
  return hostCopy;
}

std::string getSpeedGbps(int64_t speedMbps) {
  return std::to_string(speedMbps / 1000) + "G";
}

std::string getl2EntryTypeStr(L2EntryType l2EntryType) {
  switch (l2EntryType) {
    case L2EntryType::L2_ENTRY_TYPE_PENDING:
      return "Pending";
    case L2EntryType::L2_ENTRY_TYPE_VALIDATED:
      return "Validated";
    default:
      return "Unknown";
  }
}

bool comparePortName(
    const std::basic_string<char>& nameA,
    const std::basic_string<char>& nameB) {
  static const RE2 exp("([a-z][a-z][a-z])(\\d+)/(\\d+)/(\\d+)");
  std::string moduleNameA, moduleNumStrA, portNumStrA, subportNumStrA;
  std::string moduleNameB, moduleNumStrB, portNumStrB, subportNumStrB;
  if (!RE2::FullMatch(
          nameA,
          exp,
          &moduleNameA,
          &moduleNumStrA,
          &portNumStrA,
          &subportNumStrA)) {
    throw std::invalid_argument(folly::to<std::string>(
        "Invalid port name: ",
        nameA,
        "\nPort name must match 'moduleNum/port/subport' pattern"));
  }

  if (!RE2::FullMatch(
          nameB,
          exp,
          &moduleNameB,
          &moduleNumStrB,
          &portNumStrB,
          &subportNumStrB)) {
    throw std::invalid_argument(folly::to<std::string>(
        "Invalid port name: ",
        nameB,
        "\nPort name must match 'moduleNum/port/subport' pattern"));
  }

  int ret;
  if ((ret = moduleNameA.compare(moduleNameB)) != 0) {
    return ret < 0;
  }

  if (moduleNumStrA.compare(moduleNumStrB) != 0) {
    return stoi(moduleNumStrA) < stoi(moduleNumStrB);
  }

  if (portNumStrA.compare(portNumStrB) != 0) {
    return stoi(portNumStrA) < stoi(portNumStrB);
  }

  return stoi(subportNumStrA) < stoi(subportNumStrB);
}

bool compareSystemPortName(
    const std::basic_string<char>& nameA,
    const std::basic_string<char>& nameB) {
  static const RE2 exp("([^:]+):([a-z][a-z][a-z])(\\d+)/(\\d+)/(\\d+)");
  std::string switchNameA, moduleNameA, moduleNumStrA, portNumStrA,
      subportNumStrA;
  std::string switchNameB, moduleNameB, moduleNumStrB, portNumStrB,
      subportNumStrB;
  if (!RE2::FullMatch(
          nameA,
          exp,
          &switchNameA,
          &moduleNameA,
          &moduleNumStrA,
          &portNumStrA,
          &subportNumStrA)) {
    throw std::invalid_argument(folly::to<std::string>(
        "Invalid port name: ",
        nameA,
        "\nSystemPort name must match 'moduleNum/port/subport' pattern"));
  }

  if (!RE2::FullMatch(
          nameB,
          exp,
          &switchNameB,
          &moduleNameB,
          &moduleNumStrB,
          &portNumStrB,
          &subportNumStrB)) {
    throw std::invalid_argument(folly::to<std::string>(
        "Invalid port name: ",
        nameB,
        "\nSystemPort name must match 'moduleNum/port/subport' pattern"));
  }

  int ret;
  if ((ret = switchNameA.compare(switchNameB)) != 0) {
    return ret < 0;
  }

  if ((ret = moduleNameA.compare(moduleNameB)) != 0) {
    return ret < 0;
  }

  if (moduleNumStrA.compare(moduleNumStrB) != 0) {
    return stoi(moduleNumStrA) < stoi(moduleNumStrB);
  }

  if (portNumStrA.compare(portNumStrB) != 0) {
    return stoi(portNumStrA) < stoi(portNumStrB);
  }

  return stoi(subportNumStrA) < stoi(subportNumStrB);
}

std::optional<std::string> getMyHostname(const std::string& hostname) {
  if (hostname != "localhost") {
    return utils::removeFbDomains(hostname);
  }

  char actualHostname[HOST_NAME_MAX];
  if (gethostname(actualHostname, HOST_NAME_MAX) != 0) {
    return std::nullopt;
  }

  return utils::removeFbDomains(std::string(actualHostname));
}

std::string escapeDoubleQuotes(const std::string& cmd) {
  std::string cmdCopy = cmd;
  const re2::RE2 doubleQuotes(R"(")");
  re2::RE2::GlobalReplace(&cmdCopy, doubleQuotes, R"(\\")");
  return cmdCopy;
}

std::string getCmdToRun(const std::string& hostname, const std::string& cmd) {
  // For running on localhost
  //    return cmd as is.
  // For remote command request
  //    need to login with right credentials,
  //    enclose the command in double quotes
  return hostname == "localhost" ? cmd
                                 : folly::to<std::string>(
                                       "sush2 -q --reason fboss2_cli netops@",
                                       hostname,
                                       " ",
                                       "\"",
                                       escapeDoubleQuotes(cmd),
                                       "\"");
}

std::string runCmd(const std::string& cmd) {
  std::array<char, 4096> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(
      popen(cmd.c_str(), "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  return result;
}

bool isFbossFeatureEnabled(
    const std::string& hostname,
    const std::string& feature) {
  auto featureCheckCmd = utils::getCmdToRun(
      hostname,
      folly::to<std::string>(
          "file -b /etc/coop/.METADATA/features/", feature, "/current/on"));
  auto result = utils::runCmd(featureCheckCmd);
  // An empty file should be present if the feature is enabled
  return result.find("empty") != std::string::npos ? true : false;
}

std::string getSubscriptionPathStr(const fsdb::OperSubscriberInfo& subscriber) {
  if (apache::thrift::get_pointer(subscriber.path())) {
    return folly::join(
        "/", apache::thrift::get_pointer(subscriber.path())->raw().value());
  }
  std::vector<std::string> extPaths;
  if (auto subExtPaths =
          apache::thrift::get_pointer(subscriber.extendedPaths())) {
    for (const auto& extPath : *subExtPaths) {
      std::vector<std::string> pathElements;
      for (const auto& pathElm : *extPath.path()) {
        if (pathElm.any().has_value()) {
          pathElements.push_back("*");
        } else if (pathElm.regex().has_value()) {
          pathElements.push_back(*pathElm.regex());
        } else {
          pathElements.push_back(*pathElm.raw());
        }
      }
      extPaths.push_back(folly::join("/", pathElements));
    }
  }
  auto paths = apache::thrift::get_pointer(subscriber.paths());
  if (paths) {
    for (const auto& path : *paths) {
      extPaths.push_back(folly::join("/", path.second.path().value()));
    }
  }
  return folly::join(";", extPaths);
}

std::map<int16_t, std::vector<std::string>> getSwitchIndicesForInterfaces(
    const HostInfo& hostInfo,
    const std::vector<std::string>& interfaces) {
  std::map<int16_t, std::vector<std::string>> switchIndicesForInterfaces;
  try {
    auto agentClient =
        utils::createClient<apache::thrift::Client<FbossCtrl>>(hostInfo);
    agentClient->sync_getSwitchIndicesForInterfaces(
        switchIndicesForInterfaces, interfaces);
  } catch (const std::exception& e) {
    std::cerr << e.what();
  }
  return switchIndicesForInterfaces;
}

Table::StyledCell styledBer(double ber) {
  std::ostringstream outStringStream;
  outStringStream << std::setprecision(3) << ber;
  if (ber > 1e-5) {
    return Table::StyledCell(outStringStream.str(), Table::Style::ERROR);
  }
  if (ber > 1e-7) {
    return Table::StyledCell(outStringStream.str(), Table::Style::WARN);
  }
  return Table::StyledCell(outStringStream.str(), Table::Style::GOOD);
}

Table::StyledCell styledFecTail(int tail) {
  if (tail > 12) {
    return Table::StyledCell(folly::to<std::string>(tail), Table::Style::ERROR);
  }
  if (tail > 8) {
    return Table::StyledCell(folly::to<std::string>(tail), Table::Style::WARN);
  }
  return Table::StyledCell(folly::to<std::string>(tail), Table::Style::GOOD);
}

cfg::SwitchType getSwitchType(apache::thrift::Client<FbossCtrl>& client) {
  std::map<int64_t, cfg::SwitchInfo> switchIdToSwitchInfo;
  client.sync_getSwitchIdToSwitchInfo(switchIdToSwitchInfo);
  return getSwitchType(std::move(switchIdToSwitchInfo));
}

cfg::SwitchType getSwitchType(
    std::map<int64_t, cfg::SwitchInfo> switchIdToSwitchInfo) {
  CHECK_GE(switchIdToSwitchInfo.size(), 1);

  // Assert that all switches have the same switch type
  auto switchType =
      folly::copy(switchIdToSwitchInfo.begin()->second.switchType().value());
  CHECK(std::all_of(
      switchIdToSwitchInfo.begin(),
      switchIdToSwitchInfo.end(),
      [switchType](const auto& pair) {
        return pair.second.get_switchType() == switchType;
      }));

  return switchType;
}

bool isVoqOrFabric(cfg::SwitchType switchType) {
  return switchType == cfg::SwitchType::VOQ ||
      switchType == cfg::SwitchType::FABRIC;
}

bool isFabricSwitch(cfg::SwitchType switchType) {
  return switchType == cfg::SwitchType::FABRIC;
}

std::map<std::string, FabricEndpoint> getFabricEndpoints(
    const HostInfo& hostInfo) {
  std::map<std::string, FabricEndpoint> entries;

  // FbossHwCtrl thrift endpoint is available whether or not multi_switch
  // feature is enabled. Leverage it.
  // Collecting such information from HwAgent is more efficient, and thus
  // preferred as SwSwitch call would just be a passthrough.
  auto hwAgentQueryFn =
      [&entries](apache::thrift::Client<facebook::fboss::FbossHwCtrl>& client) {
        std::map<std::string, FabricEndpoint> hwagentEntries;
        client.sync_getHwFabricConnectivity(hwagentEntries);
        entries.merge(hwagentEntries);
      };
  utils::runOnAllHwAgents(hostInfo, hwAgentQueryFn);

  return entries;
}

std::map<std::string, int64_t> getAgentFb303RegexCounters(
    const HostInfo& hostInfo,
    const std::string& regex) {
  std::map<std::string, int64_t> counters;
#ifndef IS_OSS
  // TODO: sync_getRegexCounters is not available in OSS
  if (utils::isFbossFeatureEnabled(hostInfo.getName(), "multi_switch")) {
    auto hwAgentQueryFn =
        [&counters,
         &regex](apache::thrift::Client<facebook::fboss::FbossCtrl>& client) {
          std::map<std::string, int64_t> hwagentCounters;
          client.sync_getRegexCounters(hwagentCounters, regex);
          counters.merge(hwagentCounters);
        };
    utils::runOnAllHwAgents(hostInfo, hwAgentQueryFn);
  } else {
    utils::createClient<apache::thrift::Client<FbossCtrl>>(hostInfo)
        ->sync_getRegexCounters(counters, regex);
  }
#endif
  return counters;
}

std::unordered_map<std::string, std::vector<std::string>>
getCachedSwSwitchReachabilityInfo(
    const HostInfo& hostInfo,
    const std::vector<std::string>& switchNames) {
  // Query swAgent for cached reachability information
  std::unordered_map<std::string, std::vector<std::string>> reachabilityMatrix;
  auto client =
      utils::createClient<apache::thrift::Client<FbossCtrl>>(hostInfo);

  std::map<std::string, std::vector<std::string>> reachability;
  client->sync_getSwitchReachability(reachability, switchNames);
  for (auto& [switchName, reachablePorts] : reachability) {
    reachabilityMatrix[switchName].insert(
        reachabilityMatrix[switchName].end(),
        reachablePorts.begin(),
        reachablePorts.end());
  }

  return reachabilityMatrix;
}

std::unordered_map<std::string, std::vector<std::string>>
getUncachedSwitchReachabilityInfo(
    const HostInfo& hostInfo,
    const std::vector<std::string>& switchNames) {
  std::unordered_map<std::string, std::vector<std::string>> reachabilityMatrix;
  auto hwAgentQueryFn =
      [&reachabilityMatrix, &switchNames](
          apache::thrift::Client<facebook::fboss::FbossHwCtrl>& client) {
        std::map<std::string, std::vector<std::string>> reachability;
        client.sync_getHwSwitchReachability(reachability, switchNames);
        for (auto& [switchName, reachablePorts] : reachability) {
          reachabilityMatrix[switchName].insert(
              reachabilityMatrix[switchName].end(),
              reachablePorts.begin(),
              reachablePorts.end());
        }
      };

  try {
    utils::runOnAllHwAgents(hostInfo, hwAgentQueryFn);
  } catch (const std::exception& e) {
    std::cerr << e.what();
  }
  return reachabilityMatrix;
}

} // namespace facebook::fboss::utils
