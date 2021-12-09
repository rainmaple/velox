/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "velox/exec/Aggregate.h"
#include "velox/expression/FunctionSignature.h"

namespace facebook::velox::exec {

bool isRawInput(core::AggregationNode::Step step) {
  return step == core::AggregationNode::Step::kPartial ||
      step == core::AggregationNode::Step::kSingle;
}

bool isPartialOutput(core::AggregationNode::Step step) {
  return step == core::AggregationNode::Step::kPartial ||
      step == core::AggregationNode::Step::kIntermediate;
}

namespace {

struct FunctionEntry {
  std::vector<std::shared_ptr<AggregateFunctionSignature>> signatures;
  AggregateFunctionFactory factory;
};

using FunctionMap = std::unordered_map<std::string, FunctionEntry>;

FunctionMap& functions() {
  static FunctionMap functions;
  return functions;
}

std::optional<const FunctionEntry*> getAggregateFunctionEntry(
    const std::string& name) {
  auto& functionsMap = functions();
  auto it = functionsMap.find(name);
  if (it != functionsMap.end()) {
    return &it->second;
  }

  return std::nullopt;
}
} // namespace

bool registerAggregateFunction(
    const std::string& name,
    std::vector<std::shared_ptr<AggregateFunctionSignature>> signatures,
    AggregateFunctionFactory factory) {
  functions()[name] = {std::move(signatures), std::move(factory)};
  return true;
}

std::optional<std::vector<std::shared_ptr<AggregateFunctionSignature>>>
getAggregateFunctionSignatures(const std::string& name) {
  if (auto func = getAggregateFunctionEntry(name)) {
    return func.value()->signatures;
  }

  return std::nullopt;
}

std::unique_ptr<Aggregate> Aggregate::create(
    const std::string& name,
    core::AggregationNode::Step step,
    const std::vector<TypePtr>& argTypes,
    const TypePtr& resultType) {
  // Lookup the function in the new registry first.
  if (auto func = getAggregateFunctionEntry(name)) {
    return func.value()->factory(step, argTypes, resultType);
  }

  // Now check the legacy registry.
  auto func = AggregateFunctions().Create(name, step, argTypes, resultType);
  if (func.get() == nullptr) {
    std::ostringstream message;
    VELOX_USER_FAIL("Aggregate function not registered: {}", name);
  }
  return func;
}

AggregateFunctionRegistry& AggregateFunctions() {
  static AggregateFunctionRegistry instance;
  return instance;
}

} // namespace facebook::velox::exec
