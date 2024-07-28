#pragma once

namespace synapse {
namespace tofino {

const char *const MARKER_CPU_HEADER = "CPU_HEADER";
const char *const MARKER_CUSTOM_HEADERS = "CUSTOM_HEADERS";

const char *const MARKER_INGRESS_HEADERS = "INGRESS_HEADERS";
const char *const MARKER_INGRESS_METADATA = "INGRESS_METADATA";
const char *const MARKER_INGRESS_PARSER = "INGRESS_PARSER";
const char *const MARKER_INGRESS_CONTROL = "INGRESS_CONTROL";
const char *const MARKER_INGRESS_CONTROL_APPLY = "INGRESS_CONTROL_APPLY";
const char *const MARKER_INGRESS_DEPARSER = "INGRESS_DEPARSER";

const char *const MARKER_EGRESS_HEADERS = "EGRESS_HEADERS";
const char *const MARKER_EGRESS_METADATA = "EGRESS_METADATA";

const char *const TEMPLATE_FILENAME = "tofino.template.p4";
const char *const OUTPUT_FILENAME = "tofino.p4";

} // namespace tofino
} // namespace synapse