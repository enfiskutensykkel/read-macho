#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>


uint32_t read_magic(FILE* stream)
{
    uint32_t magic;

    fread(&magic, sizeof(magic), 1, stream);

    return magic;
}


bool is_object_file(FILE* stream)
{
    mach_header_64 header;
    fread(&header, sizeof(header), 1, stream);

    return header.filetype == MH_OBJECT;
}


void extract_byte_code(FILE* stream, uint8_t*& byte_code, size_t& length);
void create_executable(FILE* stream, uint8_t* byte_code, size_t length);


int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        return 1;
    }

    FILE* stream;
    if ((stream = fopen(argv[1], "r")) == NULL)
    {
        fprintf(stderr, "%s: invalid file\n", argv[1]);
        return 1;
    }

    uint32_t magic = read_magic(stream);
    if (magic != MH_MAGIC_64)
    {
        fprintf(stderr, "%s: unsupported or unrecognized format 0x%08x\n", argv[1], magic);
        fclose(stream);
        return 1;
    }

    rewind(stream);
    if (!is_object_file(stream))
    {
        fprintf(stderr, "%s: is not an object file\n", argv[1]);
        fclose(stream);
        return 1;
    }

    uint8_t* byte_code = NULL;
    size_t length = 0;
    rewind(stream);
    extract_byte_code(stream, byte_code, length);

    fclose(stream);

    if (byte_code == NULL)
    {
        fprintf(stderr, "%s: couldn't find text section in file\n", argv[1]);
        return 2;
    }

    if ((stream = fopen(argv[2], "w")) == NULL)
    {
        fprintf(stderr, "%s: couldn't open file for writing\n", argv[2]);
        fclose(stream);
        return 1;
    }

    create_executable(stream, byte_code, length);

    fclose(stream);
    free(byte_code);

    return 0;
}


void extract_text_section(FILE* stream, const segment_command_64& text_segment, uint8_t*& code, size_t& len)
{
    section_64 text_section;

    for (uint32_t i = 0; i < text_segment.nsects; ++i)
    {
        fread(&text_section, sizeof(text_section), 1, stream);
        
        if (strcmp(text_section.sectname, SECT_TEXT) == 0)
        {
            fseek(stream, text_section.offset, SEEK_SET);
            
            code = (uint8_t*) malloc(text_section.size);
            len = text_section.size;

            fread(code, 1, len, stream);
            return;
        }
    }

}


void extract_byte_code(FILE* stream, uint8_t*& code, size_t& len)
{
    mach_header_64 header;
    fread(&header, sizeof(header), 1, stream);

    segment_command_64 text_segment;

    for (uint32_t i = 0; i < header.ncmds; ++i)
    {
        long int pos = ftell(stream);

        load_command command;
        fread(&command, sizeof(command), 1, stream);

        if (command.cmd == LC_SEGMENT_64)
        {
            fseek(stream, pos, SEEK_SET);
            fread(&text_segment, sizeof(text_segment), 1, stream);

            extract_text_section(stream, text_segment, code, len);
            return;
        }

        fseek(stream, pos + command.cmdsize, SEEK_SET);
    }
}


void create_executable(FILE* stream, uint8_t* data, size_t len)
{
    mach_header_64 hdr;
    segment_command_64 seg;
    section_64 sect;
    entry_point_command entry;
    dylinker_command dyld;
    dylib_command libsys;

    segment_command_64 linkedit;
    segment_command_64 null_seg;

    hdr.magic = MH_MAGIC_64;
    hdr.cputype = CPU_TYPE_X86_64;
    hdr.cpusubtype = CPU_SUBTYPE_LIB64 | CPU_SUBTYPE_I386_ALL;
    hdr.filetype = MH_EXECUTE;
    hdr.ncmds = 6;
    hdr.sizeofcmds = sizeof(null_seg) + sizeof(linkedit) + sizeof(seg) + sizeof(sect) + sizeof(dyld) + 20 + sizeof(entry) + sizeof(libsys) + 32;
    hdr.flags = MH_NOUNDEFS;
    hdr.reserved = 0;

    null_seg.cmd = LC_SEGMENT_64;
    null_seg.cmdsize = sizeof(null_seg);
    strcpy(null_seg.segname, SEG_PAGEZERO);
    null_seg.vmaddr = 0x0;
    null_seg.vmsize = 0x1000;
    null_seg.fileoff = 0;
    null_seg.filesize = 0;
    null_seg.maxprot = VM_PROT_NONE;
    null_seg.initprot = VM_PROT_NONE;
    null_seg.nsects = 0;
    null_seg.flags = 0;

    seg.cmd = LC_SEGMENT_64;
    seg.cmdsize = sizeof(seg) + sizeof(sect);
    strcpy(seg.segname, SEG_TEXT);
    seg.vmaddr = 0x1000;
    seg.vmsize = 0x1000;
    seg.fileoff = 0; //hdr.sizeofcmds + sizeof(hdr); 
    seg.filesize = 0;
    seg.maxprot = VM_PROT_ALL;
    seg.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
    seg.nsects = 1; 
    seg.flags = 0;

    strcpy(sect.sectname, SECT_TEXT);
    strcpy(sect.segname, SEG_TEXT);
    sect.addr = 0x1000;
    sect.size = len;
    sect.offset = hdr.sizeofcmds + sizeof(hdr);
    sect.align = 4;
    sect.reloff = 0;
    sect.nreloc = 0;
    sect.flags = S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS;
    sect.reserved1 = sect.reserved2 = sect.reserved3 = 0;

    linkedit.cmd = LC_SEGMENT_64;
    linkedit.cmdsize = sizeof(linkedit);
    strcpy(linkedit.segname, SEG_LINKEDIT);
    linkedit.vmaddr = 0x9000;
    linkedit.vmsize = 0x1000;
    linkedit.fileoff = 0;
    linkedit.filesize = 0;
    linkedit.maxprot = VM_PROT_ALL;
    linkedit.initprot = VM_PROT_DEFAULT;
    linkedit.nsects = 0;
    linkedit.flags = 0;

    dyld.cmd = LC_LOAD_DYLINKER;
    dyld.cmdsize = sizeof(dyld) + 20;
    dyld.name.offset = 12;

    libsys.cmd = LC_LOAD_DYLIB;
    libsys.cmdsize = sizeof(libsys) + 32;
    libsys.dylib.name.offset = 24;
    libsys.dylib.timestamp = 2;
    libsys.dylib.current_version = 0x4ca0a01;
    libsys.dylib.compatibility_version = 0x10000;

    entry.cmd = LC_MAIN;
    entry.cmdsize = sizeof(entry);
    entry.entryoff = hdr.sizeofcmds + sizeof(hdr);
    entry.stacksize = 0;

    fwrite(&hdr, sizeof(hdr), 1, stream);
    fwrite(&null_seg, sizeof(null_seg), 1, stream);
    fwrite(&seg, sizeof(seg), 1, stream);
    fwrite(&sect, sizeof(sect), 1, stream);
    fwrite(&linkedit, sizeof(linkedit), 1, stream);
    fwrite(&dyld, sizeof(dyld), 1, stream);
    fwrite("/usr/lib/dyld\x00\x00\x00\x00\x00\x00\x00", 1, 20, stream);
    fwrite(&entry, sizeof(entry), 1, stream);
    fwrite(&libsys, sizeof(libsys), 1, stream);
    fwrite("/usr/lib/libSystem.B.dylib\x00\x00\x00\x00\x00\x00", 1, 32, stream);

    fwrite(data, 1, len, stream);

//    for (size_t i = 0; i < (64ULL - len); ++i)
//    {
//        fwrite("\x00", 1, 1, stream);
//    }

    for (size_t i = 0; i < 0x1000; ++i)
    {
        fwrite("\x00", 1, 1, stream);
    }
}
