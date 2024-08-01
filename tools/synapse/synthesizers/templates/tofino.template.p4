#include <core.p4>

#if __TARGET_TOFINO__ == 2
    #include <t2na.p4>
    #define CPU_PCIE_PORT 0
    #define RECIRCULATION_PORT 6
#else
    #include <tna.p4>
    #define CPU_PCIE_PORT 192
    #define RECIRCULATION_PORT 320
#endif

typedef bit<9> port_t;
typedef bit<7> port_pad_t;

typedef bit<8> match_counter_t;

header cpu_h {
    bit<16> code_path;
    
    @padding port_pad_t pad_in_port;
    port_t in_port;

    @padding port_pad_t pad_out_port;
    port_t out_port;

/*@{CPU_HEADER}@*/
}

/*@{CUSTOM_HEADERS}@*/

struct my_ingress_headers_t {
    cpu_h cpu;
/*@{INGRESS_HEADERS}@*/
}

struct my_ingress_metadata_t {
/*@{INGRESS_METADATA}@*/
}

struct my_egress_headers_t {
    cpu_h cpu;
/*@{EGRESS_HEADERS}@*/
}

struct my_egress_metadata_t {
/*@{EGRESS_METADATA}@*/
}


parser TofinoIngressParser(
    packet_in pkt,
    out ingress_intrinsic_metadata_t ig_intr_md
) {
    state start {
        pkt.extract(ig_intr_md);
        transition select(ig_intr_md.resubmit_flag) {
            1 : parse_resubmit;
            0 : parse_port_metadata;
        }
    }

    state parse_resubmit {
        // Parse resubmitted packet here.
        transition reject;
    }

    state parse_port_metadata {
        pkt.advance(PORT_METADATA_SIZE);
        transition accept;
    }
}

parser IngressParser(
    packet_in pkt,

    /* User */    
    out my_ingress_headers_t hdr,
    out my_ingress_metadata_t meta,

    /* Intrinsic */
    out ingress_intrinsic_metadata_t ig_intr_md
) {
    TofinoIngressParser() tofino_parser;
    
    /* This is a mandatory state, required by Tofino Architecture */
    state start {
        tofino_parser.apply(pkt, ig_intr_md);

        transition select(ig_intr_md.ingress_port) {
            CPU_PCIE_PORT: parse_cpu;
            default: parser_init;
        }
    }

    state parse_cpu {
        pkt.extract(hdr.cpu);
        transition accept;
    }

/*@{INGRESS_PARSER}@*/
}

control Ingress(
    /* User */
    inout my_ingress_headers_t hdr,
    inout my_ingress_metadata_t meta,

    /* Intrinsic */
    in    ingress_intrinsic_metadata_t ig_intr_md,
    in    ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md
) {
    action drop() {
        ig_dprsr_md.drop_ctl = 1;
    }

    action fwd(port_t port) {
        ig_tm_md.ucast_egress_port = port;
    }

    action send_to_controller(bit<16> code_path) {
        hdr.cpu.setValid();
        hdr.cpu.code_path = code_path;
        hdr.cpu.in_port = ig_intr_md.ingress_port;
        fwd(CPU_PCIE_PORT);
    }

/*@{INGRESS_CONTROL}@*/
    apply {
/*@{INGRESS_CONTROL_APPLY}@*/
    }
}

control IngressDeparser(
    packet_out pkt,

    /* User */
    inout my_ingress_headers_t hdr,
    in    my_ingress_metadata_t meta,

    /* Intrinsic */
    in    ingress_intrinsic_metadata_for_deparser_t  ig_dprsr_md
) {
    apply {
/*@{INGRESS_DEPARSER}@*/
    }
}

parser TofinoEgressParser(
    packet_in pkt,
    out egress_intrinsic_metadata_t eg_intr_md
) {
    state start {
        pkt.extract(eg_intr_md);
        transition accept;
    }
}

parser EgressParser(
    packet_in pkt,

    /* User */
    out my_egress_headers_t hdr,
    out my_egress_metadata_t eg_md,

    /* Intrinsic */
    out egress_intrinsic_metadata_t eg_intr_md
) {
    TofinoEgressParser() tofino_parser;

    /* This is a mandatory state, required by Tofino Architecture */
    state start {
        tofino_parser.apply(pkt, eg_intr_md);
        transition accept;
    }
}

control Egress(
    /* User */
    inout my_egress_headers_t hdr,
    inout my_egress_metadata_t eg_md,

    /* Intrinsic */
    in    egress_intrinsic_metadata_t eg_intr_md,
    in    egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
    inout egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md,
    inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md
) {
    apply {}
}

control EgressDeparser(
    packet_out pkt,

    /* User */
    inout my_egress_headers_t hdr,
    in    my_egress_metadata_t eg_md,

    /* Intrinsic */
    in egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md
) {
    apply {
        pkt.emit(hdr);
    }
}
Pipeline(
    IngressParser(),
    Ingress(),
    IngressDeparser(),
    EgressParser(),
    Egress(),
    EgressDeparser()
) pipe;

Switch(pipe) main;
