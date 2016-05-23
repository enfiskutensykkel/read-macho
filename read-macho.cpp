#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>
#include <vector>
#include <map>
#include <memory>
#include <string>


typedef std::shared_ptr<segment_command_64> segment_64_ptr;
typedef std::vector<segment_64_ptr> vec_segment_64;

typedef std::shared_ptr<section_64> section_64_ptr;
typedef std::vector<section_64_ptr> vec_section_64;
typedef std::map<segment_64_ptr, vec_section_64> map_section_64;


void parse_64bit_mach(FILE* stream);


uint32_t read_magic(FILE* stream)
{
    uint32_t magic;
    fread(&magic, sizeof(magic), 1, stream);
    return magic;
}


int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <object file or executable>\n", argv[0]);
        return 1;
    }

    FILE* stream = fopen(argv[1], "r");
    if (stream == NULL)
    {
        fprintf(stderr, "%s: invalid file\n", argv[1]);
        return 1;
    }

    uint32_t signature = read_magic(stream);
    rewind(stream);
    
    switch (signature)
    {
        case MH_MAGIC_64:
            parse_64bit_mach(stream);
            break;

        default:
            fprintf(stderr, "%s: unsupported or unrecognized format 0x%08x\n", argv[1], signature);
            break;
    }

    fclose(stream);
    return 0;
}


void load_segment_64(FILE* stream, vec_segment_64& segments, map_section_64& segment_sections)
{
    segment_64_ptr segment(new segment_command_64);
    fread(segment.get(), sizeof(segment_command_64), 1, stream);
    segments.push_back(segment);

    auto& sections = segment_sections[segment];

    for (uint32_t section_idx = 0; section_idx < segment->nsects; ++section_idx)
    {
        section_64_ptr section(new section_64);
        fread(section.get(), sizeof(section_64), 1, stream);
        sections.push_back(section);
    }
}


void parse_64bit_mach(FILE* stream)
{
    mach_header_64 header;
    fread(&header, sizeof(header), 1, stream);
    
    vec_segment_64 segments;
    map_section_64 sections;

    entry_point_command entry_point;
    dylib_command dylib;

    for (uint32_t i = 0; i < header.ncmds; ++i)
    {
        long int pos = ftell(stream);

        load_command command;
        fread(&command, sizeof(command), 1, stream);
        fseek(stream, pos, SEEK_SET);

        switch (command.cmd)
        {
            case LC_SEGMENT_64:
                load_segment_64(stream, segments, sections);
                break;

            case LC_MAIN:
                fread(&entry_point, sizeof(entry_point_command), 1, stream);
                break;

            case LC_LOAD_DYLIB:
                fread(&dylib, sizeof(dylib), 1, stream);
                fprintf(stdout, "%x\n", dylib.dylib.current_version);
                break;
        }
        
        fseek(stream, pos + command.cmdsize, SEEK_SET);
    }

    if (header.filetype == MH_EXECUTE)
    {
        segment_64_ptr text_seg;

        for (auto& segment: segments)
        {
            if (strcmp(segment->segname, SEG_TEXT) == 0)
            {
                text_seg = segment;
                break;
            }
        }

        if (text_seg.get() != nullptr)
        {
            //fseek(stream, text_seg->fileoff + entry_point.entryoff, SEEK_SET);
            fseek(stream, entry_point.entryoff, SEEK_SET);

            for (int i = 0; i < 32; ++i)
            {
                int data = fgetc(stream);
                fprintf(stdout, "%02x ", data);
            }
            fprintf(stdout, "\n");
        }
    }
}
