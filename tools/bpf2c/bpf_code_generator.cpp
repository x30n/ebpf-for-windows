// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

// Whenever bpf code generate output changes, bpf2c_tests will fail unless the
// expected files in tests\bpf2c_tests\expected are updated. The following
// script can be used to regenerate the expected files:
//     generate_expected_bpf2c_output.ps1
//
// Usage:
// .\scripts\generate_expected_bpf2c_output.ps1 <build_output_path>
// Example:
// .\scripts\generate_expected_bpf2c_output.ps1 .\x64\Debug\

#include <iostream>
#include <vector>

#include "btf_parser.h"
#include "bpf_code_generator.h"

#if !defined(_countof)
#define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif
#include <cassert>

static const std::string _register_names[11] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9",
    "r10",
};

enum class AluOperations
{
    Add,
    Sub,
    Mul,
    Div,
    Or,
    And,
    Lsh,
    Rsh,
    Neg,
    Mod,
    Xor,
    Mov,
    Ashr,
    ByteOrder,
};

static const std::string _predicate_format_string[] = {
    "",                           // JA
    "%s == %s",                   // JEQ
    "%s > %s",                    // JGT
    "%s >= %s",                   // JGE
    "%s & %s",                    // JSET
    "%s != %s",                   // JNE
    "(int64_t)%s > (int64_t)%s",  // JSGT
    "(int64_t)%s >= (int64_t)%s", // JSGE
    "",                           // CALL
    "",                           // EXIT
    "%s < %s",                    // JLT
    "%s <= %s",                   // JLE
    "(int64_t)%s < (int64_t)%s",  // JSLT
    "(int64_t)%s <= (int64_t)%s", // JSLE
};

#define ADD_OPCODE(X)                            \
    {                                            \
        static_cast<uint8_t>(X), std::string(#X) \
    }
static std::map<uint8_t, std::string> _opcode_name_strings = {
    ADD_OPCODE(EBPF_OP_ADD_IMM),    ADD_OPCODE(EBPF_OP_ADD_REG),   ADD_OPCODE(EBPF_OP_SUB_IMM),
    ADD_OPCODE(EBPF_OP_SUB_REG),    ADD_OPCODE(EBPF_OP_MUL_IMM),   ADD_OPCODE(EBPF_OP_MUL_REG),
    ADD_OPCODE(EBPF_OP_DIV_IMM),    ADD_OPCODE(EBPF_OP_DIV_REG),   ADD_OPCODE(EBPF_OP_OR_IMM),
    ADD_OPCODE(EBPF_OP_OR_REG),     ADD_OPCODE(EBPF_OP_AND_IMM),   ADD_OPCODE(EBPF_OP_AND_REG),
    ADD_OPCODE(EBPF_OP_LSH_IMM),    ADD_OPCODE(EBPF_OP_LSH_REG),   ADD_OPCODE(EBPF_OP_RSH_IMM),
    ADD_OPCODE(EBPF_OP_RSH_REG),    ADD_OPCODE(EBPF_OP_NEG),       ADD_OPCODE(EBPF_OP_MOD_IMM),
    ADD_OPCODE(EBPF_OP_MOD_REG),    ADD_OPCODE(EBPF_OP_XOR_IMM),   ADD_OPCODE(EBPF_OP_XOR_REG),
    ADD_OPCODE(EBPF_OP_MOV_IMM),    ADD_OPCODE(EBPF_OP_MOV_REG),   ADD_OPCODE(EBPF_OP_ARSH_IMM),
    ADD_OPCODE(EBPF_OP_ARSH_REG),   ADD_OPCODE(EBPF_OP_LE),        ADD_OPCODE(EBPF_OP_BE),
    ADD_OPCODE(EBPF_OP_ADD64_IMM),  ADD_OPCODE(EBPF_OP_ADD64_REG), ADD_OPCODE(EBPF_OP_SUB64_IMM),
    ADD_OPCODE(EBPF_OP_SUB64_REG),  ADD_OPCODE(EBPF_OP_MUL64_IMM), ADD_OPCODE(EBPF_OP_MUL64_REG),
    ADD_OPCODE(EBPF_OP_DIV64_IMM),  ADD_OPCODE(EBPF_OP_DIV64_REG), ADD_OPCODE(EBPF_OP_OR64_IMM),
    ADD_OPCODE(EBPF_OP_OR64_REG),   ADD_OPCODE(EBPF_OP_AND64_IMM), ADD_OPCODE(EBPF_OP_AND64_REG),
    ADD_OPCODE(EBPF_OP_LSH64_IMM),  ADD_OPCODE(EBPF_OP_LSH64_REG), ADD_OPCODE(EBPF_OP_RSH64_IMM),
    ADD_OPCODE(EBPF_OP_RSH64_REG),  ADD_OPCODE(EBPF_OP_NEG64),     ADD_OPCODE(EBPF_OP_MOD64_IMM),
    ADD_OPCODE(EBPF_OP_MOD64_REG),  ADD_OPCODE(EBPF_OP_XOR64_IMM), ADD_OPCODE(EBPF_OP_XOR64_REG),
    ADD_OPCODE(EBPF_OP_MOV64_IMM),  ADD_OPCODE(EBPF_OP_MOV64_REG), ADD_OPCODE(EBPF_OP_ARSH64_IMM),
    ADD_OPCODE(EBPF_OP_ARSH64_REG), ADD_OPCODE(EBPF_OP_LDXW),      ADD_OPCODE(EBPF_OP_LDXH),
    ADD_OPCODE(EBPF_OP_LDXB),       ADD_OPCODE(EBPF_OP_LDXDW),     ADD_OPCODE(EBPF_OP_STW),
    ADD_OPCODE(EBPF_OP_STH),        ADD_OPCODE(EBPF_OP_STB),       ADD_OPCODE(EBPF_OP_STDW),
    ADD_OPCODE(EBPF_OP_STXW),       ADD_OPCODE(EBPF_OP_STXH),      ADD_OPCODE(EBPF_OP_STXB),
    ADD_OPCODE(EBPF_OP_STXDW),      ADD_OPCODE(EBPF_OP_LDDW),      ADD_OPCODE(EBPF_OP_JA),
    ADD_OPCODE(EBPF_OP_JEQ_IMM),    ADD_OPCODE(EBPF_OP_JEQ_REG),   ADD_OPCODE(EBPF_OP_JGT_IMM),
    ADD_OPCODE(EBPF_OP_JGT_REG),    ADD_OPCODE(EBPF_OP_JGE_IMM),   ADD_OPCODE(EBPF_OP_JGE_REG),
    ADD_OPCODE(EBPF_OP_JSET_REG),   ADD_OPCODE(EBPF_OP_JSET_IMM),  ADD_OPCODE(EBPF_OP_JNE_IMM),
    ADD_OPCODE(EBPF_OP_JNE_REG),    ADD_OPCODE(EBPF_OP_JSGT_IMM),  ADD_OPCODE(EBPF_OP_JSGT_REG),
    ADD_OPCODE(EBPF_OP_JSGE_IMM),   ADD_OPCODE(EBPF_OP_JSGE_REG),  ADD_OPCODE(EBPF_OP_CALL),
    ADD_OPCODE(EBPF_OP_EXIT),       ADD_OPCODE(EBPF_OP_JLT_IMM),   ADD_OPCODE(EBPF_OP_JLT_REG),
    ADD_OPCODE(EBPF_OP_JLE_IMM),    ADD_OPCODE(EBPF_OP_JLE_REG),   ADD_OPCODE(EBPF_OP_JSLT_IMM),
    ADD_OPCODE(EBPF_OP_JSLT_REG),   ADD_OPCODE(EBPF_OP_JSLE_IMM),  ADD_OPCODE(EBPF_OP_JSLE_REG),
};

std::string
bpf_code_generator::get_register_name(uint8_t id)
{
    if (id >= _countof(_register_names)) {
        throw std::runtime_error("Invalid register id");
    } else {
        current_section->referenced_registers.insert(_register_names[id]);
        return _register_names[id];
    }
}

bpf_code_generator::bpf_code_generator(const std::string& path, const std::string& c_name)
    : current_section(nullptr), c_name(c_name), path(path)
{
    if (!reader.load(path)) {
        throw std::runtime_error(std::string("Can't process ELF file ") + path);
    }

    extract_btf_information();
}

bpf_code_generator::bpf_code_generator(const std::string& c_name, const std::vector<ebpf_inst>& instructions)
    : c_name(c_name)
{
    current_section = &sections[c_name];
    get_register_name(0);
    get_register_name(1);
    get_register_name(10);
    uint32_t offset = 0;
    for (const auto& instruction : instructions) {
        current_section->output.push_back({instruction, offset++});
    }
}

std::vector<std::string>
bpf_code_generator::program_sections()
{
    std::vector<std::string> section_names;
    for (const auto& section : reader.sections) {
        std::string name = section->get_name();
        if (name.empty() || name[0] == '.')
            continue;
        if ((section->get_type() == 1) && (section->get_flags() == 6)) {
            section_names.push_back(section->get_name());
        }
    }
    return section_names;
}

void
bpf_code_generator::parse(const std::string& section_name, const GUID& program_type, const GUID& attach_type)
{
    current_section = &sections[section_name];
    get_register_name(0);
    get_register_name(1);
    get_register_name(10);

    set_program_and_attach_type(program_type, attach_type);
    extract_program(section_name);
    extract_relocations_and_maps(section_name);
}

void
bpf_code_generator::set_program_and_attach_type(const GUID& program_type, const GUID& attach_type)
{
    memcpy(&current_section->program_type, &program_type, sizeof(GUID));
    memcpy(&current_section->expected_attach_type, &attach_type, sizeof(GUID));
}

void
bpf_code_generator::generate(const std::string& section_name)
{
    current_section = &sections[section_name];

    generate_labels();
    build_function_table();
    encode_instructions(section_name);
}

void
bpf_code_generator::extract_program(const std::string& section_name)
{
    auto program_section = reader.sections[section_name];
    std::vector<ebpf_inst> program{
        reinterpret_cast<const ebpf_inst*>(program_section->get_data()),
        reinterpret_cast<const ebpf_inst*>(program_section->get_data() + program_section->get_size())};

    ELFIO::const_symbol_section_accessor symbols{reader, reader.sections[".symtab"]};
    for (ELFIO::Elf_Xword index = 0; index < symbols.get_symbols_num(); index++) {
        std::string name{};
        ELFIO::Elf64_Addr value{};
        ELFIO::Elf_Xword size{};
        unsigned char bind{};
        unsigned char symbol_type{};
        ELFIO::Elf_Half section_index{};
        unsigned char other{};
        symbols.get_symbol(index, name, value, size, bind, symbol_type, section_index, other);
        if (name.empty()) {
            continue;
        }
        if (section_index == program_section->get_index() && value == 0) {
            current_section->program_name = name;
            break;
        }
    }

    uint32_t offset = 0;
    for (const auto& instruction : program) {
        current_section->output.push_back({instruction, offset++});
    }
}

void
bpf_code_generator::parse()
{
    auto map_section = reader.sections["maps"];
    ELFIO::const_symbol_section_accessor symbols{reader, reader.sections[".symtab"]};

    if (map_section) {
        size_t data_size = map_section->get_size();

        if (data_size % sizeof(ebpf_map_definition_in_file_t) != 0) {
            throw std::runtime_error(
                std::string("bad maps section size, must be a multiple of ") +
                std::to_string(sizeof(ebpf_map_definition_in_file_t)));
        }

        for (ELFIO::Elf_Xword i = 0; i < symbols.get_symbols_num(); i++) {
            std::string symbol_name;
            ELFIO::Elf64_Addr symbol_value{};
            unsigned char symbol_bind{};
            unsigned char symbol_type{};
            ELFIO::Elf_Half symbol_section_index{};
            unsigned char symbol_other{};
            ELFIO::Elf_Xword symbol_size{};

            symbols.get_symbol(
                i,
                symbol_name,
                symbol_value,
                symbol_size,
                symbol_bind,
                symbol_type,
                symbol_section_index,
                symbol_other);

            if (symbol_section_index == map_section->get_index()) {
                if (symbol_size != sizeof(ebpf_map_definition_in_file_t)) {
                    throw std::runtime_error("invalid map size");
                }
                map_definitions[symbol_name].definition =
                    *reinterpret_cast<const ebpf_map_definition_in_file_t*>(map_section->get_data() + symbol_value);

                map_definitions[symbol_name].index = symbol_value / sizeof(ebpf_map_definition_in_file_t);
            }
        }
    }
}

void
bpf_code_generator::extract_relocations_and_maps(const std::string& section_name)
{
    auto map_section = reader.sections["maps"];
    ELFIO::const_symbol_section_accessor symbols{reader, reader.sections[".symtab"]};

    auto relocations = reader.sections[std::string(".rel") + section_name];
    if (!relocations)
        relocations = reader.sections[std::string(".rela") + section_name];

    if (relocations) {
        ELFIO::const_relocation_section_accessor relocation_reader{reader, relocations};
        ELFIO::Elf_Xword relocation_count = relocation_reader.get_entries_num();
        for (ELFIO::Elf_Xword index = 0; index < relocation_count; index++) {
            ELFIO::Elf64_Addr offset{};
            ELFIO::Elf_Word symbol{};
            unsigned char type{};
            ELFIO::Elf_Sxword addend{};
            relocation_reader.get_entry(index, offset, symbol, type, addend);
            {
                std::string name{};
                ELFIO::Elf64_Addr value{};
                ELFIO::Elf_Xword size{};
                unsigned char bind{};
                unsigned char symbol_type{};
                ELFIO::Elf_Half section_index{};
                unsigned char other{};
                if (!symbols.get_symbol(symbol, name, value, size, bind, symbol_type, section_index, other)) {
                    throw std::runtime_error(
                        std::string("Can't perform relocation at offset ") + std::to_string(offset));
                }
                current_section->output[offset / sizeof(ebpf_inst)].relocation = name;
                if (map_section && section_index == map_section->get_index()) {
                    // Check that the map exists in the list of map definitions.
                    if (map_definitions.find(name) == map_definitions.end()) {
                        throw std::runtime_error(std::string("map not found in map definitions: ") + name);
                    }
                }
            }
        }
    }
}

void
bpf_code_generator::extract_btf_information()
{
    auto btf = reader.sections[".BTF"];
    auto btf_ext = reader.sections[".BTF.ext"];

    if (btf == nullptr) {
        return;
    }

    if (btf_ext == nullptr) {
        return;
    }
    std::vector<uint8_t> btf_data(
        reinterpret_cast<const uint8_t*>(btf->get_data()),
        reinterpret_cast<const uint8_t*>(btf->get_data()) + btf->get_size());
    std::vector<uint8_t> btf_ext_data(
        reinterpret_cast<const uint8_t*>(btf_ext->get_data()),
        reinterpret_cast<const uint8_t*>(btf_ext->get_data()) + btf_ext->get_size());

    btf_parse_line_information(
        btf_data,
        btf_ext_data,
        [&section_line_info = this->section_line_info](
            const std::string& section,
            uint32_t instruction_offset,
            const std::string& file_name,
            const std::string& source,
            uint32_t line_number,
            uint32_t column_number) {
            line_info_t info{file_name, source, line_number, column_number};
            section_line_info[section].emplace(instruction_offset / sizeof(ebpf_inst), info);
        });
}

void
bpf_code_generator::generate_labels()
{
    std::vector<output_instruction_t>& program_output = current_section->output;

    // Tag jump targets
    for (size_t i = 0; i < program_output.size(); i++) {
        auto& output = program_output[i];
        if ((output.instruction.opcode & EBPF_CLS_MASK) != EBPF_CLS_JMP) {
            continue;
        }
        if (output.instruction.opcode == EBPF_OP_CALL) {
            continue;
        }
        if (output.instruction.opcode == EBPF_OP_EXIT) {
            continue;
        }
        program_output[i + output.instruction.offset + 1].jump_target = true;
    }

    // Add labels to instructions that are targets of jumps
    size_t label_index = 1;
    for (auto& output : program_output) {
        if (!output.jump_target) {
            continue;
        }
        output.label = std::string("label_") + std::to_string(label_index++);
    }
}

void
bpf_code_generator::build_function_table()
{
    std::vector<output_instruction_t>& program_output = current_section->output;

    // Gather helper_functions
    size_t index = 0;
    for (auto& output : program_output) {
        if (output.instruction.opcode != EBPF_OP_CALL) {
            continue;
        }
        std::string name;
        if (!output.relocation.empty()) {
            name = output.relocation;
        } else {
            name = "helper_id_";
            name += std::to_string(output.instruction.imm);
        }

        if (current_section->helper_functions.find(name) == current_section->helper_functions.end()) {
            current_section->helper_functions[name] = {output.instruction.imm, index++};
        }
    }
}

void
bpf_code_generator::encode_instructions(const std::string& section_name)
{
    std::vector<output_instruction_t>& program_output = current_section->output;
    auto program_name = !current_section->program_name.empty() ? current_section->program_name : section_name;
    auto helper_array_prefix = program_name + "_helpers[%s]";

    // Encode instructions
    for (size_t i = 0; i < program_output.size(); i++) {
        auto& output = program_output[i];
        auto& inst = output.instruction;

        switch (inst.opcode & EBPF_CLS_MASK) {
        case EBPF_CLS_ALU:
        case EBPF_CLS_ALU64: {
            std::string destination = get_register_name(inst.dst);
            std::string source;
            if (inst.opcode & EBPF_SRC_REG)
                source = get_register_name(inst.src);
            else
                source = std::string("IMMEDIATE(") + std::to_string(inst.imm) + std::string(")");
            bool is64bit = (inst.opcode & EBPF_CLS_MASK) == EBPF_CLS_ALU64;
            AluOperations operation = static_cast<AluOperations>(inst.opcode >> 4);
            std::string check_div_by_zero =
                format_string("if (%s == 0) { division_by_zero(%s); return -1; }", source, std::to_string(i));
            std::string swap_function;
            switch (operation) {
            case AluOperations::Add:
                output.lines.push_back(format_string("%s += %s;", destination, source));
                break;
            case AluOperations::Sub:
                output.lines.push_back(format_string("%s -= %s;", destination, source));
                break;
            case AluOperations::Mul:
                output.lines.push_back(format_string("%s *= %s;", destination, source));
                break;
            case AluOperations::Div:
                output.lines.push_back(check_div_by_zero);
                if (is64bit)
                    output.lines.push_back(format_string("%s /= %s;", destination, source));
                else
                    output.lines.push_back(
                        format_string("%s = (uint32_t)%s / (uint32_t)%s;", destination, destination, source));
                break;
            case AluOperations::Or:
                output.lines.push_back(format_string("%s |= %s;", destination, source));
                break;
            case AluOperations::And:
                output.lines.push_back(format_string("%s &= %s;", destination, source));
                break;
            case AluOperations::Lsh:
                output.lines.push_back(format_string("%s <<= %s;", destination, source));
                break;
            case AluOperations::Rsh:
                if (is64bit)
                    output.lines.push_back(format_string("%s >>= %s;", destination, source));
                else
                    output.lines.push_back(format_string("%s = (uint32_t)%s >> %s;", destination, destination, source));
                break;
            case AluOperations::Neg:
                if (is64bit)
                    output.lines.push_back(format_string("%s = -%s;", destination, destination));
                else
                    output.lines.push_back(format_string("%s = -(int64_t)%s;", destination, destination));
                break;
            case AluOperations::Mod:
                output.lines.push_back(check_div_by_zero);
                if (is64bit)
                    output.lines.push_back(format_string("%s %%= %s;", destination, source));
                else
                    output.lines.push_back(
                        format_string("%s = (uint32_t)%s %% (uint32_t)%s;", destination, destination, source));
                break;
            case AluOperations::Xor:
                output.lines.push_back(format_string("%s ^= %s;", destination, source));
                break;
            case AluOperations::Mov:
                output.lines.push_back(format_string("%s = %s;", destination, source));
                break;
            case AluOperations::Ashr:
                if (is64bit)
                    output.lines.push_back(
                        format_string("%s = (int64_t)%s >> (uint32_t)%s;", destination, destination, source));
                else
                    output.lines.push_back(format_string("%s = (int32_t)%s >> %s;", destination, destination, source));
                break;
            case AluOperations::ByteOrder: {
                std::string size_type = "";
                if (output.instruction.opcode & EBPF_SRC_REG) {
                    switch (inst.imm) {
                    case 16:
                        swap_function = "htobe16";
                        size_type = "uint16_t";
                        break;
                    case 32:
                        swap_function = "htobe32";
                        size_type = "uint32_t";
                        break;
                    case 64:
                        is64bit = true;
                        size_type = "uint64_t";
                        swap_function = "htobe64";
                        break;
                    default:
                        throw std::runtime_error("invalid operand");
                    }
                } else {
                    switch (inst.imm) {
                    case 16:
                        swap_function = "htole16";
                        size_type = "uint16_t";
                        break;
                    case 32:
                        swap_function = "htole32";
                        size_type = "uint32_t";
                        break;
                    case 64:
                        is64bit = true;
                        swap_function = "htole64";
                        size_type = "uint64_t";
                        break;
                    default:
                        throw std::runtime_error("invalid operand");
                    }
                }
                output.lines.push_back(
                    format_string("%s = %s((%s)%s);", destination, swap_function, size_type, destination));
            } break;
            default:
                throw std::runtime_error("invalid operand");
            }
            if (!is64bit)
                output.lines.push_back(format_string("%s &= UINT32_MAX;", destination));

        } break;
        case EBPF_CLS_LD: {
            i++;
            if (inst.opcode != EBPF_OP_LDDW) {
                throw std::runtime_error("invalid operand");
            }
            std::string destination = get_register_name(inst.dst);
            if (output.relocation.empty()) {
                uint64_t imm = static_cast<uint32_t>(program_output[i].instruction.imm);
                imm <<= 32;
                imm |= static_cast<uint32_t>(output.instruction.imm);
                std::string source;
                source = std::string("(uint64_t)") + std::to_string(imm);
                output.lines.push_back(format_string("%s = %s;", destination, source));
            } else {
                std::string source;
                auto map_definition = map_definitions.find(output.relocation);
                if (map_definition == map_definitions.end()) {
                    throw std::runtime_error(std::string("Map ") + output.relocation + std::string(" doesn't exist"));
                }
                source = format_string("_maps[%s].address", std::to_string(map_definition->second.index));
                output.lines.push_back(format_string("%s = POINTER(%s);", destination, source));
                current_section->referenced_map_indices.insert(map_definitions[output.relocation].index);
            }
        } break;
        case EBPF_CLS_LDX: {
            std::string size_type;
            std::string destination = get_register_name(inst.dst);
            std::string source = get_register_name(inst.src);
            std::string offset = std::string("OFFSET(") + std::to_string(inst.offset) + ")";
            switch (inst.opcode & EBPF_SIZE_DW) {
            case EBPF_SIZE_B:
                size_type = std::string("uint8_t");
                break;
            case EBPF_SIZE_H:
                size_type = std::string("uint16_t");
                break;
            case EBPF_SIZE_W:
                size_type = std::string("uint32_t");
                break;
            case EBPF_SIZE_DW:
                size_type = std::string("uint64_t");
                break;
            }
            output.lines.push_back(
                format_string("%s = *(%s *)(uintptr_t)(%s + %s);", destination, size_type, source, offset));
        } break;
        case EBPF_CLS_ST:
        case EBPF_CLS_STX: {
            std::string size_type;
            std::string destination = get_register_name(inst.dst);
            std::string source;
            if ((inst.opcode & EBPF_CLS_MASK) == EBPF_CLS_ST) {
                source = std::string("IMMEDIATE(") + std::to_string(inst.imm) + std::string(")");
            } else {
                source = get_register_name(inst.src);
            }
            std::string offset = std::string("OFFSET(") + std::to_string(inst.offset) + ")";
            switch (inst.opcode & EBPF_SIZE_DW) {
            case EBPF_SIZE_B:
                size_type = std::string("uint8_t");
                break;
            case EBPF_SIZE_H:
                size_type = std::string("uint16_t");
                break;
            case EBPF_SIZE_W:
                size_type = std::string("uint32_t");
                break;
            case EBPF_SIZE_DW:
                size_type = std::string("uint64_t");
                break;
            }
            source = std::string("(") + size_type + std::string(")") + source;
            output.lines.push_back(
                format_string("*(%s *)(uintptr_t)(%s + %s) = %s;", size_type, destination, offset, source));
        } break;
        case EBPF_CLS_JMP: {
            std::string destination = get_register_name(inst.dst);
            std::string source;
            if (inst.opcode & EBPF_SRC_REG) {
                source = get_register_name(inst.src);
            } else {
                source = std::string("IMMEDIATE(") + std::to_string(inst.imm) + std::string(")");
            }
            auto& format = _predicate_format_string[inst.opcode >> 4];
            if (inst.opcode == EBPF_OP_JA) {
                std::string target = program_output[i + inst.offset + 1].label;
                output.lines.push_back(std::string("goto ") + target + std::string(";"));
            } else if (inst.opcode == EBPF_OP_CALL) {
                std::string function_name;
                if (output.relocation.empty()) {
                    function_name = format_string(
                        helper_array_prefix,
                        std::to_string(
                            current_section
                                ->helper_functions[std::string("helper_id_") + std::to_string(output.instruction.imm)]
                                .index));
                } else {
                    auto helper_function = current_section->helper_functions.find(output.relocation);
                    assert(helper_function != current_section->helper_functions.end());
                    function_name = format_string(
                        helper_array_prefix,
                        std::to_string(current_section->helper_functions[output.relocation].index));
                }
                output.lines.push_back(
                    get_register_name(0) + std::string(" = ") + function_name + std::string(".address"));
                output.lines.push_back(
                    std::string("(") + get_register_name(1) + std::string(", ") + get_register_name(2) +
                    std::string(", ") + get_register_name(3) + std::string(", ") + get_register_name(4) +
                    std::string(", ") + get_register_name(5) + std::string(");"));
                output.lines.push_back(
                    format_string("if ((%s.tail_call) && (%s == 0)) return 0;", function_name, get_register_name(0)));
            } else if (inst.opcode == EBPF_OP_EXIT) {
                output.lines.push_back(std::string("return ") + get_register_name(0) + std::string(";"));
            } else {
                std::string target = program_output[i + inst.offset + 1].label;
                if (target.empty()) {
                    throw std::runtime_error("invalid jump target");
                }
                std::string predicate = format_string(format, destination, source);
                output.lines.push_back(format_string("if (%s) goto %s;", predicate, target));
            }
        } break;
        default:
            throw std::runtime_error("invalid operand");
        }
    }
}

void
bpf_code_generator::emit_c_code(std::ostream& output_stream)
{
    // Emit C file
    output_stream << "#include \"bpf2c.h\"" << std::endl << std::endl;

    // Emit import tables
    if (map_definitions.size() > 0) {
        output_stream << "static map_entry_t _maps[] = {" << std::endl;
        size_t map_size = map_definitions.size();
        size_t current_index = 0;
        while (current_index < map_size) {
            for (const auto& [name, entry] : map_definitions) {
                if (entry.index == current_index) {
                    output_stream << "{ NULL, { ";
                    output_stream << entry.definition.type << ", ";
                    output_stream << entry.definition.key_size << ", ";
                    output_stream << entry.definition.value_size << ", ";
                    output_stream << entry.definition.max_entries << ", ";
                    output_stream << entry.definition.inner_map_idx << ", ";
                    output_stream << entry.definition.pinning << ", ";
                    output_stream << entry.definition.id << ", ";
                    output_stream << entry.definition.inner_id << ", ";
                    output_stream << " }, \"" << name.c_str() << "\" }," << std::endl;

                    current_index++;
                    break;
                }
            }
        }
        output_stream << "};" << std::endl;
        output_stream << std::endl;
        output_stream
            << "static void _get_maps(_Outptr_result_buffer_maybenull_(*count) map_entry_t** maps, _Out_ size_t* count)"
            << std::endl;
        output_stream << "{" << std::endl;
        output_stream << "\t*maps = _maps;" << std::endl;
        output_stream << "\t*count = " << std::to_string(map_definitions.size()) << ";" << std::endl;
        output_stream << "}" << std::endl;
        output_stream << std::endl;
    } else {
        output_stream
            << "static void _get_maps(_Outptr_result_buffer_maybenull_(*count) map_entry_t** maps, _Out_ size_t* count)"
            << std::endl;
        output_stream << "{" << std::endl;
        output_stream << "\t*maps = NULL;" << std::endl;
        output_stream << "\t*count = 0;" << std::endl;
        output_stream << "}" << std::endl;
        output_stream << std::endl;
    }

    for (auto& [name, section] : sections) {
        auto program_name = !section.program_name.empty() ? section.program_name : name;

        if (section.output.size() == 0) {
            continue;
        }

        // Emit section specific helper function array.
        if (section.helper_functions.size() > 0) {
            std::string helper_array_name = program_name + "_helpers";
            output_stream << "static helper_function_entry_t " << sanitize_name(helper_array_name) << "[] = {"
                          << std::endl;

            // Functions are emitted in the order in which they occur in the byte code.
            std::vector<std::tuple<std::string, uint32_t>> index_ordered_helpers;
            index_ordered_helpers.resize(section.helper_functions.size());
            for (const auto& function : section.helper_functions) {
                index_ordered_helpers[function.second.index] = std::make_tuple(function.first, function.second.id);
            }

            for (const auto& [helper_name, id] : index_ordered_helpers) {
                output_stream << "{ NULL, " << id << ", \"" << helper_name.c_str() << "\"}," << std::endl;
            }

            output_stream << "};" << std::endl;
            output_stream << std::endl;
        }

        // Emit the program and attach type GUID.
        std::string program_type_name = program_name + "_program_type_guid";
        std::string attach_type_name = program_name + "_attach_type_guid";

#if defined(_MSC_VER)
        output_stream << format_string(
                             "static GUID %s = %s;",
                             sanitize_name(program_type_name),
                             format_guid(&section.program_type))
                      << std::endl;
        output_stream << format_string(
                             "static GUID %s = %s;",
                             sanitize_name(attach_type_name),
                             format_guid(&section.expected_attach_type))
                      << std::endl;
#endif

        if (section.referenced_map_indices.size() > 0) {
            // Emit the array for the maps used.
            std::string map_array_name = program_name + "_maps";
            output_stream << format_string("static uint16_t %s[] = {", sanitize_name(map_array_name)) << std::endl;
            for (const auto& map_index : section.referenced_map_indices) {
                output_stream << std::to_string(map_index) << "," << std::endl;
            }
            output_stream << "};" << std::endl;
            output_stream << std::endl;
        }

        auto& line_info = section_line_info[name];
        auto first_line_info = line_info.find(section.output.front().instruction_offset);
        std::string prolog_line_info;
        if (first_line_info != line_info.end() && !first_line_info->second.file_name.empty()) {
            prolog_line_info = format_string(
                "#line %s \"%s\"\n",
                std::to_string(first_line_info->second.line_number),
                escape_string(first_line_info->second.file_name));
        }

        // Emit entry point
        output_stream << format_string("static uint64_t %s(void* context)", sanitize_name(program_name)) << std::endl;
        output_stream << "{" << std::endl;

        // Emit prologue
        output_stream << prolog_line_info << "\t// Prologue" << std::endl;
        output_stream << prolog_line_info << "\tuint64_t stack[(UBPF_STACK_SIZE + 7) / 8];" << std::endl;
        for (const auto& r : _register_names) {
            // Skip unused registers
            if (section.referenced_registers.find(r) == section.referenced_registers.end()) {
                continue;
            }
            output_stream << prolog_line_info << "\tregister uint64_t " << r.c_str() << " = 0;" << std::endl;
        }
        output_stream << std::endl;
        output_stream << prolog_line_info << "\t" << get_register_name(1) << " = (uintptr_t)context;" << std::endl;
        output_stream << prolog_line_info << "\t" << get_register_name(10)
                      << " = (uintptr_t)((uint8_t*)stack + sizeof(stack));" << std::endl;
        output_stream << std::endl;

        // Emit encoded instructions.
        for (const auto& output : section.output) {
            if (output.lines.empty()) {
                continue;
            }
            if (!output.label.empty())
                output_stream << output.label << ":" << std::endl;
            auto current_line = line_info.find(output.instruction_offset);
            if (current_line != line_info.end() && !current_line->second.file_name.empty() &&
                current_line->second.line_number != 0) {
                prolog_line_info = format_string(
                    "#line %s \"%s\"\n",
                    std::to_string(current_line->second.line_number),
                    escape_string(current_line->second.file_name));
            }
#if defined(_DEBUG) || defined(BPF2C_VERBOSE)
            output_stream << "\t// " << _opcode_name_strings[output.instruction.opcode]
                          << " pc=" << output.instruction_offset << " dst=" << get_register_name(output.instruction.dst)
                          << " src=" << get_register_name(output.instruction.src)
                          << " offset=" << std::to_string(output.instruction.offset)
                          << " imm=" << std::to_string(output.instruction.imm) << std::endl;
#endif
            for (const auto& line : output.lines) {
                output_stream << prolog_line_info << "\t" << line << std::endl;
            }
        }
        // Emit epilogue
        output_stream << prolog_line_info << "}" << std::endl;
        output_stream << "#line __LINE__ __FILE__" << std::endl << std::endl;
    }

    output_stream << "static program_entry_t _programs[] = {" << std::endl;
    for (auto& [name, program] : sections) {
        auto program_name = !program.program_name.empty() ? program.program_name : name;
        size_t map_count = program.referenced_map_indices.size();
        size_t helper_count = program.helper_functions.size();
        auto map_array_name = map_count ? (program_name + "_maps") : std::string("NULL");
        auto helper_array_name = helper_count ? (program_name + "_helpers") : std::string("NULL");
#if defined(_MSC_VER)
        auto program_type_guid_name = program_name + "_program_type_guid";
        auto attach_type_guid_name = program_name + "_attach_type_guid";
#else
        auto program_type_guid_name = std::string("NULL");
        auto attach_type_guid_name = std::string("NULL");
#endif
        output_stream << "\t{ " << sanitize_name(program_name) << ", "
                      << "\"" << name.c_str() << "\", "
                      << "\"" << program.program_name.c_str() << "\", " << map_array_name.c_str() << ", "
                      << program.referenced_map_indices.size() << ", " << helper_array_name.c_str() << ", "
                      << program.helper_functions.size() << ", " << program.output.size() << ", "
#if defined(_MSC_VER)
                      << "&" << sanitize_name(program_type_guid_name) << ", "
                      << "&" << sanitize_name(attach_type_guid_name) << ", "
#else
                      << sanitize_name(program_type_guid_name) << ", " << sanitize_name(attach_type_guid_name) << ", "
#endif
                      << "}," << std::endl;
    }
    output_stream << "};" << std::endl;
    output_stream << std::endl;
    output_stream
        << "static void _get_programs(_Outptr_result_buffer_(*count) program_entry_t** programs, _Out_ size_t* count)"
        << std::endl;
    output_stream << "{" << std::endl;
    output_stream << "\t*programs = _programs;" << std::endl;
    output_stream << "\t*count = " << std::to_string(sections.size()) << ";" << std::endl;
    output_stream << "}" << std::endl;
    output_stream << std::endl;

    output_stream << std::endl;
    output_stream << format_string(
        "metadata_table_t %s = { _get_programs, _get_maps };\n", c_name.c_str() + std::string("_metadata_table"));
}

#if defined(_MSC_VER)
std::string
bpf_code_generator::format_guid(const GUID* guid)
{
    std::string output(120, '\0');
    std::string format_string =
        "{0x%08x, 0x%04x, 0x%04x, {0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x}}";
    auto count = snprintf(
        output.data(),
        output.size(),
        format_string.c_str(),
        guid->Data1,
        guid->Data2,
        guid->Data3,
        guid->Data4[0],
        guid->Data4[1],
        guid->Data4[2],
        guid->Data4[3],
        guid->Data4[4],
        guid->Data4[5],
        guid->Data4[6],
        guid->Data4[7]);
    if (count < 0) {
        throw std::runtime_error("Error formatting GUID");
    }

    output.resize(strlen(output.c_str()));
    return output;
}
#endif

std::string
bpf_code_generator::format_string(
    const std::string& format,
    const std::string insert_1,
    const std::string insert_2,
    const std::string insert_3,
    const std::string insert_4)
{
    std::string output(200, '\0');
    if (insert_2.empty()) {
        auto count = snprintf(output.data(), output.size(), format.c_str(), insert_1.c_str());
        if (count < 0)
            throw std::runtime_error("Error formatting string");
    } else if (insert_3.empty()) {
        auto count = snprintf(output.data(), output.size(), format.c_str(), insert_1.c_str(), insert_2.c_str());
        if (count < 0)
            throw std::runtime_error("Error formatting string");
    }
    if (insert_4.empty()) {
        auto count = snprintf(
            output.data(), output.size(), format.c_str(), insert_1.c_str(), insert_2.c_str(), insert_3.c_str());
        if (count < 0)
            throw std::runtime_error("Error formatting string");
    } else {
        auto count = snprintf(
            output.data(),
            output.size(),
            format.c_str(),
            insert_1.c_str(),
            insert_2.c_str(),
            insert_3.c_str(),
            insert_4.c_str());
        if (count < 0)
            throw std::runtime_error("Error formatting string");
    }
    output.resize(strlen(output.c_str()));
    return output;
}

std::string
bpf_code_generator::sanitize_name(const std::string& name)
{
    std::string safe_name = name;
    for (auto& c : safe_name) {
        if (!isalnum(c)) {
            c = '_';
        }
    }
    return safe_name;
}

std::string
bpf_code_generator::escape_string(const std::string& input)
{
    std::string output;
    for (const auto& c : input) {
        if (c != '\\') {
            output += c;
        } else {
            output += "\\\\";
        }
    }
    return output;
}
