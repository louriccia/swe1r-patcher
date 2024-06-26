/*

  Star Wars Episode 1: Racer - Patcher

  (c)2018 Jannik Vogel

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>


#define USE_PATCHED_GUID 0
#define USE_PATCHED_FONTS 1
#define USE_TRIGGER_DISPLAY 0
#define USE_R100 1


#ifdef LOADER

#include <windows.h>

#ifdef DLL

typedef void* Target;

static void writex(Target target, off_t offset, const void* data, size_t size) {
  //MessageBoxA(NULL, "Meep", "Meep X", 0);
  void* address = (void*)(uintptr_t)offset;
  DWORD old_protect;
  VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protect);
  memcpy(address, data, size);
  VirtualProtect(address, size, old_protect, &old_protect);
  return;
}

static void readx(Target target, off_t offset, void* data, size_t size) {
  memcpy(data, (void*)(uintptr_t)offset, size);
  return;
}

#else

typedef struct {
  PROCESS_INFORMATION process_information;
} Target;

static void writex(Target target, off_t offset, const void* data, size_t size) {
  void* address = (void*)(uintptr_t)offset;
  DWORD old_protect;
  VirtualProtectEx(target.process_information.hProcess, address, size, PAGE_EXECUTE_READWRITE, &old_protect);
  BOOL status = WriteProcessMemory(target.process_information.hProcess, address, data, size, NULL);
  assert(status != 0);
  VirtualProtectEx(target.process_information.hProcess, address, size, old_protect, &old_protect);
  return;
}

static void readx(Target target, off_t offset, void* data, size_t size) {
  ReadProcessMemory(target.process_information.hProcess, (void*)(uintptr_t)offset, data, size, NULL);
  return;
}

#endif

#else

static off_t mapExe(uint32_t offset) {
  /*
    From `objdump -x swep1rcr.exe` for the patched US version:

    0 .text         000aa750  00401000  00401000  00000400  2**2
                    CONTENTS, ALLOC, LOAD, READONLY, CODE
    1 .rdata        000054a2  004ac000  004ac000  000aac00  2**2
                    CONTENTS, ALLOC, LOAD, READONLY, DATA
    2 .data         00023600  004b2000  004b2000  000b0200  2**2
                    CONTENTS, ALLOC, LOAD, DATA
    3 .rsrc         000017b8  00ece000  00ece000  000d3800  2**2
  */

  assert(offset >= 0x400000);

  if (offset >= 0x00ed0000) {
    // hack
    offset -= 0x00ed0000;
    offset += 0x000d5000;
  } else if (offset >= 0x00ece000) {
    // .rsrc
    offset -= 0x00ece000;
    offset += 0x000d3800;
  } else if (offset >= 0x004b2000) {
   // .data
    offset -= 0x004b2000;
    offset += 0x000b0200;
  } else if (offset >= 0x004ac000) {
    // .rdata
    offset -= 0x004ac000;
    offset += 0x000aac00;
  } else if (offset >= 0x00401000) {
    // .text
    offset -= 0x00401000;
    offset += 0x00000400;
  } else {
    // headers
    offset -= 0x00400000;
  }
  return offset;
}

typedef struct {
  FILE* f;
} Target;

static void writex(Target target, off_t offset, const void* data, size_t size) {
  fseek(target.f, mapExe(offset), SEEK_SET);
  fwrite(data, size, 1, target.f);
  return;
}

static void readx(Target target, off_t offset, void* data, size_t size) {
  fseek(target.f, mapExe(offset), SEEK_SET);
  fread(data, size, 1, target.f);
  return;
}

#endif

static uint8_t read8(Target target, off_t offset) {
  uint8_t value;
  readx(target, offset, &value, 1);
  return value;
}

static void write8(Target target, off_t offset, uint8_t value) {
  writex(target, offset, &value, 1);
  return;
}

static uint16_t read16(Target target, off_t offset) {
  uint16_t value;
  readx(target, offset, &value, 2);
  return value;
}

static void write16(Target target, off_t offset, uint16_t value) {
  writex(target, offset, &value, 2);
  return;
}

static uint32_t read32(Target target, off_t offset) {
  uint32_t value;
  readx(target, offset, &value, 4);
  return value;
}

static void write32(Target target, off_t offset, uint32_t value) {
  writex(target, offset, &value, 4);
  return;
}

static void patch16_add(Target target, off_t offset, uint16_t delta) {
  write16(target, offset, read16(target, offset) + delta);
  return;
}

static void patch32_add(Target target, off_t offset, uint32_t delta) {
  write32(target, offset, read32(target, offset) + delta);
  return;
}

static uint32_t add_esp(Target target, uint32_t memory_offset, int32_t n) {
  write8(target, memory_offset, 0x81); memory_offset += 1;
  write8(target, memory_offset, 0xC4); memory_offset += 1;
  write32(target, memory_offset, (uint32_t)n); memory_offset += 4;
  return memory_offset;
}

static uint32_t test_eax_eax(Target target, uint32_t memory_offset) {
  write8(target, memory_offset, 0x85); memory_offset += 1;
  write8(target, memory_offset, 0xC0); memory_offset += 1;
  return memory_offset;
}

static uint32_t test_edx_edx(Target target, uint32_t memory_offset) {
  write8(target, memory_offset, 0x85); memory_offset += 1;
  write8(target, memory_offset, 0xD2); memory_offset += 1;
  return memory_offset;
}

static uint32_t nop(Target target, uint32_t memory_offset) {
  write8(target, memory_offset, 0x90); memory_offset += 1;
  return memory_offset;
}

static uint32_t push_eax(Target target, uint32_t memory_offset) {
  write8(target, memory_offset, 0x50); memory_offset += 1;
  return memory_offset;
}

static uint32_t push_edx(Target target, uint32_t memory_offset) {
  write8(target, memory_offset, 0x52); memory_offset += 1;
  return memory_offset;
}

static uint32_t pop_edx(Target target, uint32_t memory_offset) {
  write8(target, memory_offset, 0x5A); memory_offset += 1;
  return memory_offset;
}

static uint32_t push_u32(Target target, uint32_t memory_offset, uint32_t value) {
  write8(target, memory_offset, 0x68); memory_offset += 1;
  write32(target, memory_offset, value); memory_offset += 4;
  return memory_offset;
}

static uint32_t call(Target target, uint32_t memory_offset, uint32_t address) {
  write8(target, memory_offset, 0xE8); memory_offset += 1;
  write32(target, memory_offset, address - (memory_offset + 4)); memory_offset += 4;
  return memory_offset;
}

static uint32_t jmp(Target target, uint32_t memory_offset, uint32_t address) {
  write8(target, memory_offset, 0xE9); memory_offset += 1;
  write32(target, memory_offset, address - (memory_offset + 4)); memory_offset += 4;
  return memory_offset;
}

static uint32_t jnz(Target target, uint32_t memory_offset, uint32_t address) {
  write8(target, memory_offset, 0x0F); memory_offset += 1;
  write8(target, memory_offset, 0x85); memory_offset += 1;
  write32(target, memory_offset, address - (memory_offset + 4)); memory_offset += 4;
  return memory_offset;
}

static uint32_t retn(Target target, uint32_t memory_offset) {
  write8(target, memory_offset, 0xC3); memory_offset += 1;
  return memory_offset;
}

#if 0

static void* readExe(Target target, uint32_t offset, size_t size) {

  // Find correct location in binary
  offset = offset;

  // Allocate space for the read
  void* data = malloc(size);

  // Read the data from the exe
  off_t previous_offset = ftell(f);
  fseek(target, offset, SEEK_SET);
  fread(data, size, 1, f);
  fseek(target, previous_offset, SEEK_SET);

  return data;
}

static void dumpTexture(Target target, size_t offset, uint8_t unk0, uint8_t unk1, unsigned int width, unsigned int height, const char* filename) {
  // Presumably the format information?
  assert(unk0 == 3);
  assert(unk1 == 0);

  FILE* out = fopen(filename, "wb");
  fprintf(out, "P3\n%d %d\n15\n", width, height);

  // Copy the pixel data
  uint8_t* texture = readExe(target, offset, width * height * 4 / 8);
  for(unsigned int i = 0; i < width * height * 2; i++) {
    uint8_t v = ((texture[i / 2] << ((i % 2) * 4)) & 0xF0) >> 4;
    fprintf(out, "%d %d %d\n", v, v, v);
  }
  free(texture);

  fclose(out);
  return;
}

static uint32_t dumpTextureTable(Target target, uint32_t offset, uint8_t unk0, uint8_t unk1, unsigned int width, unsigned int height, const char* filename) {
  char filename_i[4096]; //FIXME: Size

  // Get size of the table
  uint32_t* buffer = readExe(target, offset + 0, 4);
  uint32_t count = *buffer;
  free(buffer);

  // Loop over elements and dump each
  printf("// %s at 0x%X\n", filename, offset);
  uint32_t* offsets = readExe(target, offset + 4, count * 4);
  for(unsigned int i = 0; i < count; i++) {
    sprintf(filename_i, "%s_%d.ppm", filename, i);
    printf("// - [%d]: 0x%X\n", i, offsets[i]);
    dumpTexture(target, offsets[i], unk0, unk1, width, height, filename_i);
  }
  free(offsets);
  return count;
}

#endif

static uint32_t patchTextureTable(Target target, uint32_t memory_offset, uint32_t offset, uint32_t code_begin_offset, uint32_t code_end_offset, uint32_t width, uint32_t height, const char* filename) {

#if 1
  // Attempt to realign the disassembler
  for(unsigned int i = 0; i < 16; i++) {
    memory_offset = nop(target, memory_offset);
  }
#endif

  // Create a code cave
  // The original argument for the width is only 8 bit (signed), so it's hard
  // to extend. That's why we use a code cave.
  uint32_t cave_memory_offset = memory_offset;

  // Patches the arguments for the texture loader
  memory_offset = push_u32(target, memory_offset, height);
  memory_offset = push_u32(target, memory_offset, width);
  memory_offset = push_u32(target, memory_offset, height);
  memory_offset = push_u32(target, memory_offset, width);
  memory_offset = jmp(target, memory_offset, code_end_offset);

  //FIXME: Fixup the format?
  //.text:0042D794                 push    0
  //.text:0042D796                 push    3

  // Write code to jump into the codecave (5 bytes) and clear original code
  uint32_t hack_offset = jmp(target, code_begin_offset, cave_memory_offset);
  printf("Tying to jump to 0x%08X\n", cave_memory_offset);
  //FIXME: Use a while loop instead
  for(unsigned int i = 5; i < (code_end_offset - code_begin_offset); i++) {
    hack_offset = nop(target, hack_offset);
  }

  // Get number of textures in the table
  uint32_t count = read32(target, offset + 0);

  // Have a buffer for pixeldata
  unsigned int texture_size = width * height * 4 / 8;
  uint8_t* buffer = malloc(texture_size);

  // Loop over all textures
  for(unsigned int i = 0; i < count; i++) {

    // Load input texture to buffer
    char path[4096];
    sprintf(path, "textures/%s_%d_test.data", filename, i);
    printf("Loading '%s'\n", path);
    FILE* ft = fopen(path, "rb");
    assert(ft != NULL);
    memset(buffer, 0x00, texture_size);
    for(unsigned int i = 0; i < texture_size * 2; i++) {
      uint8_t pixel[2]; // GIMP only exports Gray + Alpha..
      fread(pixel, sizeof(pixel), 1, ft);
      buffer[i / 2] |= (pixel[0] & 0xF0) >> ((i % 2) * 4);
    }
    fclose(ft);

    // Write pixel data to game
    uint32_t texture_new = memory_offset;
    writex(target, memory_offset, buffer, texture_size);
    memory_offset += texture_size;

    // Patch the table entry
    uint32_t texture_old = read32(target, offset + 4 + i * 4);
    write32(target, offset + 4 + i * 4, texture_new);
    printf("%d: 0x%X -> 0x%X\n", i, texture_old, texture_new);
  }
  free(buffer);

  return memory_offset;
}

static void modify_network_guid(Target target, const void* data, size_t size) {

  // Patches the game GUID so people don't cheat with it (as easily)

  if (size == 0) {
    size = strlen(data);
  }

  #define SWAP(a, b) if (a ^ b) {a ^= b; b ^= a; a ^= b;}

  static uint8_t s[256];
  static bool initialized = false;
  if (!initialized) {

    // Initialize the RC4 S-Box
    for (int i = 0; i < 256; i++) {
      s[i] = i;
    }

    initialized = true;
  }

  // Modify the hash using RC4 schedule
  assert(size <= 256);
  const uint8_t* data_bytes = data;
  uint8_t j = 0;
  for (int i = 0; i < 256; i++) {
    j += s[i] + data_bytes[i % size];
    SWAP(s[i], s[j]);
  }

  // Emit 16 bytes of the keystream
  uint8_t k_i = 0;
  uint8_t k_j = 0;
  uint8_t k_s[256];
  memcpy(k_s, s, 256);
  for(int i = 0; i < 16; i++) {
    k_j += k_s[++k_i];
    SWAP(k_s[k_i], k_s[k_j]);
    uint8_t rc4_output = k_s[(k_s[k_i] + k_s[k_j]) & 0xFF];
    write8(target, 0x4AF9B0 + i, rc4_output);
  }

  // Overwrite the first 2 byte with a version index, so we have room
  // to fix the algorithm if we have messed up
  write16(target, 0x4AF9B0 + 0, 0x00000000);

  return;
}

static uint32_t patch_network_upgrades(Target target, uint32_t memory_offset, uint8_t* upgrade_levels, uint8_t* upgrade_healths) {
  // Upgrade network play updates to 100%

#if USE_PATCHED_GUID
  modify_network_guid(target, "Upgrades", 0);
  modify_network_guid(target, upgrade_levels, 7);
  modify_network_guid(target, upgrade_healths, 7);
#endif

#if 0
  // what the hell is the point of this shit, jay
  // The following patch only supports the same upgrade level and health for menus
  // So in order to keep everything synchronized, we assert that only one setting is present for all categories
  for(int i = 0; i < 7; i++) {
    assert(upgrade_levels[i] == upgrade_levels[0]);
    assert(upgrade_healths[i] == upgrade_healths[0]);
  }
#endif

  // Now do the actual upgrade for menus
  write8(target, 0x45CFC6, upgrade_levels[0]);
  write8(target, 0x45CFCB, upgrade_healths[0]);

  //FIXME: Upgrade network player creation

  // 0x45B725 vs 0x45B9FF

  #if 0
  lea     edx, [esp+1Ch+upgrade_health]
  lea     eax, [esp+1Ch+upgrade_level]
  push    edx             ; upgrade_healths
  push    eax             ; upgrade_levels
  push    ebp             ; handling_in
  push    offset handling_out ; handling_out
  mov     [esp+esi+2Ch+upgrade_health], cl
  call    _sub_449D00_generate_upgraded_handling_table_data
  #endif

  // Place upgrade data in memory

  uint32_t memory_offset_upgrade_levels = memory_offset;
  writex(target, memory_offset, upgrade_levels, 7);
  memory_offset += 7;

  uint32_t memory_offset_upgrade_healths = memory_offset;
  writex(target, memory_offset, upgrade_healths, 7);
  memory_offset += 7;


  // Now inject the code

  uint32_t memory_offset_upgrade_code = memory_offset;

  //  -> push edx
  write8(target, memory_offset, 0x52); memory_offset += 1;

  //  -> push eax
  write8(target, memory_offset, 0x50); memory_offset += 1;

  //  -> push offset upgrade_healths
  write8(target, memory_offset, 0x68); memory_offset += 1;
  write32(target, memory_offset, memory_offset_upgrade_healths); memory_offset += 4;

  //  -> push offset upgrade_levels
  write8(target, memory_offset, 0x68); memory_offset += 1;
  write32(target, memory_offset, memory_offset_upgrade_levels); memory_offset += 4;

  //  -> push esi
  write8(target, memory_offset, 0x56); memory_offset += 1;

  //  -> push edi
  write8(target, memory_offset, 0x57); memory_offset += 1;

  //  -> call _sub_449D00
  write8(target, memory_offset, 0xE8); memory_offset += 1;
  write32(target, memory_offset, 0x449D00 - (memory_offset + 4)); memory_offset += 4;

  //  -> add esp, 0x10
  write8(target, memory_offset, 0x83); memory_offset += 1;
  write8(target, memory_offset, 0xC4); memory_offset += 1;
  write8(target, memory_offset, 0x10); memory_offset += 1;

  //  -> pop eax
  write8(target, memory_offset, 0x58); memory_offset += 1;

  //  -> pop edx
  write8(target, memory_offset, 0x5A); memory_offset += 1;

  //  -> retn
  write8(target, memory_offset, 0xC3); memory_offset += 1;


  // Install it by jumping from 0x45B765 and returning to 0x45B76C

  write8(target, 0x45B765 + 0, 0xE8);
  write32(target, 0x45B765 + 1, memory_offset_upgrade_code - (0x45B765 + 5));
  write8(target, 0x45B765 + 5, 0x90);
  write8(target, 0x45B765 + 6, 0x90);

  return memory_offset;
}

static uint32_t patch_network_collisions(Target target, uint32_t memory_offset) {
  // Disable collision between network players

#if USE_PATCHED_GUID
  modify_network_guid(target, "Collisions", 0);
#endif

  // Inject the code

  uint32_t memory_offset_collision_code = memory_offset;

  memory_offset = push_edx(target, memory_offset);

  // -> mov     edx, _dword_4D5E00_is_multiplayer
  write8(target, memory_offset, 0x8B); memory_offset += 1;
  write8(target, memory_offset, 0x15); memory_offset += 1;
  write32(target, memory_offset, 0x4D5E00); memory_offset += 4;

  memory_offset = test_edx_edx(target, memory_offset);
  memory_offset = pop_edx(target, memory_offset);

  // -> jz _sub_47B0C0
  write8(target, memory_offset, 0x0F); memory_offset += 1;
  write8(target, memory_offset, 0x84); memory_offset += 1;
  write32(target, memory_offset, 0x47B0C0 - (memory_offset + 4)); memory_offset += 4;

  memory_offset = retn(target, memory_offset);


  // Install it by patching call at 0x47B5AF

  write32(target, 0x47B5AF + 1, memory_offset_collision_code - (0x47B5AF + 5));

  return memory_offset;
}

static uint32_t patch_audio_stream_quality(Target target, uint32_t memory_offset, uint32_t samplerate, uint8_t bits_per_sample, bool stereo) {
  // Patch audio streaming quality

  // Calculate a fitting buffer-size
  uint32_t buffer_size = 2 * samplerate * (bits_per_sample / 8) * (stereo ? 2 : 1);

  // Patch audio stream source setting
  write32(target, 0x423215, buffer_size);
  write8(target, 0x42321A, bits_per_sample);
  write32(target, 0x42321E, samplerate);

  // Patch audio stream buffer chunk size
  write32(target, 0x423549, buffer_size / 2);
  write32(target, 0x42354E, buffer_size / 2);
  write32(target, 0x423555, buffer_size / 2);

  return memory_offset;
}

static uint32_t patch_sprite_loader_to_load_tga(Target target, uint32_t memory_offset) {
  // Replace the sprite loader with a version that checks for "data\\images\\sprite-%d.tga"

  // Write the path we want to use to the binary
  const char* tga_path = "data\\sprites\\sprite-%d.tga";

  uint32_t memory_offset_tga_path = memory_offset;
  writex(target, memory_offset, tga_path, strlen(tga_path) + 1);
  memory_offset += strlen(tga_path) + 1;



  // FIXME: load_success: Yay! Shift down size, to compensate for higher resolution
  uint32_t memory_offset_load_success = memory_offset;
  #if 1

  // Shift the width and height of the sprite to the right
  #if 1
  write8(target, memory_offset, 0x66); memory_offset += 1;
  write8(target, memory_offset, 0xC1); memory_offset += 1;
  write8(target, memory_offset, 0x68); memory_offset += 1;
  write8(target, memory_offset, 0); memory_offset += 1;
  write8(target, memory_offset, 1); memory_offset += 1;

  write8(target, memory_offset, 0x66); memory_offset += 1;
  write8(target, memory_offset, 0xC1); memory_offset += 1;
  write8(target, memory_offset, 0x68); memory_offset += 1;
  write8(target, memory_offset, 2); memory_offset += 1;
  write8(target, memory_offset, 2); memory_offset += 1;

  write8(target, memory_offset, 0x66); memory_offset += 1;
  write8(target, memory_offset, 0xC1); memory_offset += 1;
  write8(target, memory_offset, 0x68); memory_offset += 1;
  write8(target, memory_offset, 14); memory_offset += 1;
  write8(target, memory_offset, 2); memory_offset += 1;
  #endif

  // Get address of page and repeat steps
  write8(target, memory_offset, 0x8B); memory_offset += 1;
  write8(target, memory_offset, 0x50); memory_offset += 1;
  write8(target, memory_offset, 16); memory_offset += 1;

  #if 1
  write8(target, memory_offset, 0x66); memory_offset += 1;
  write8(target, memory_offset, 0xC1); memory_offset += 1;
  write8(target, memory_offset, 0x6A); memory_offset += 1;
  write8(target, memory_offset, 0); memory_offset += 1;
  write8(target, memory_offset, 1); memory_offset += 1;

  write8(target, memory_offset, 0x66); memory_offset += 1;
  write8(target, memory_offset, 0xC1); memory_offset += 1;
  write8(target, memory_offset, 0x6A); memory_offset += 1;
  write8(target, memory_offset, 2); memory_offset += 1;
  write8(target, memory_offset, 2); memory_offset += 1;
  #endif

  // Get address of texture and repeat steps

  //0:  8b 50 10                mov    edx,DWORD PTR [eax+0x10]
  //3:  66 c1 6a 02 02          shr    WORD PTR [edx+0x2],0x2
  #endif

  // finish: Clear stack and return
  uint32_t memory_offset_finish = memory_offset;
  memory_offset = add_esp(target, memory_offset, 0x4 + 0x400);
  memory_offset = retn(target, memory_offset);

  // Start of actual code
  uint32_t memory_offset_tga_loader_code = memory_offset;

  // Read the sprite_index from stack
  //  -> mov     eax, [esp+4]
  write8(target, memory_offset, 0x8B); memory_offset += 1;
  write8(target, memory_offset, 0x44); memory_offset += 1;
  write8(target, memory_offset, 0x24); memory_offset += 1;
  write8(target, memory_offset, 0x04); memory_offset += 1;

  // Make room for sprintf buffer and keep the pointer in edx
  //  -> add     esp, -400h
  memory_offset = add_esp(target, memory_offset, -0x400);
  //  -> mov     edx, esp
  write8(target, memory_offset, 0x89); memory_offset += 1;
  write8(target, memory_offset, 0xE2); memory_offset += 1;

  // Generate the path, keep sprite_index on stack as we'll keep using it
  memory_offset = push_eax(target, memory_offset); // (sprite_index)
  memory_offset = push_u32(target, memory_offset, memory_offset_tga_path); // (fmt)
  memory_offset = push_edx(target, memory_offset); // (buffer)
  memory_offset = call(target, memory_offset, 0x49EB80); // sprintf
  memory_offset = pop_edx(target, memory_offset); // (buffer)
  memory_offset = add_esp(target, memory_offset, 0x4);

  // Attempt to load the TGA, then remove path from stack
  memory_offset = push_edx(target, memory_offset); // (buffer)
  memory_offset = call(target, memory_offset, 0x4114D0); // load_sprite_from_tga_and_add_loaded_sprite
  memory_offset = add_esp(target, memory_offset, 0x4);

  // Check if the load failed
  memory_offset = test_eax_eax(target, memory_offset);
  //  -> jnz load_success
  memory_offset = jnz(target, memory_offset, memory_offset_load_success);

  // Load failed, so load the original sprite (sprite-index still on stack)
  memory_offset = call(target, memory_offset, 0x446CA0); // load_sprite_internal

  //  -> jmp finish
  memory_offset = jmp(target, memory_offset, memory_offset_finish);


  // Install it by jumping from 0x446FB0 (and we'll return directly)
  jmp(target, 0x446FB0, memory_offset_tga_loader_code);

  return memory_offset;
}

static uint32_t patch_trigger_display(Target target, uint32_t memory_offset) {
  // Display triggers

  const char* trigger_string = "Trigger %d activated";
  float trigger_string_display_duration = 3.0f;

  uint32_t memory_offset_trigger_string = memory_offset;
  writex(target, memory_offset, trigger_string, strlen(trigger_string));
  memory_offset += strlen(trigger_string) + 1;

  uint32_t memory_offset_trigger_code = memory_offset;

  // Read the trigger from stack
  //  -> mov     eax, [esp+4]
  write8(target, memory_offset, 0x8B); memory_offset += 1;
  write8(target, memory_offset, 0x44); memory_offset += 1;
  write8(target, memory_offset, 0x24); memory_offset += 1;
  write8(target, memory_offset, 0x04); memory_offset += 1;

  // Get pointer to section 8
  //0:  8b 40 4c                mov    eax,DWORD PTR [eax+0x4c]
  write8(target, memory_offset, 0x8B); memory_offset += 1;
  write8(target, memory_offset, 0x40); memory_offset += 1;
  write8(target, memory_offset, 0x4C); memory_offset += 1;

  // Read the section8.trigger_action field
  //3:  0f b7 40 24             movzx  eax,WORD PTR [eax+0x24] 
  write8(target, memory_offset, 0x0F); memory_offset += 1;
  write8(target, memory_offset, 0xB7); memory_offset += 1;
  write8(target, memory_offset, 0x40); memory_offset += 1;
  write8(target, memory_offset, 0x24); memory_offset += 1;

  // Make room for sprintf buffer and keep the pointer in edx
  //  -> add     esp, -400h
  memory_offset = add_esp(target, memory_offset, -0x400);
  //  -> mov     edx, esp
  write8(target, memory_offset, 0x89); memory_offset += 1;
  write8(target, memory_offset, 0xE2); memory_offset += 1;

  // Generate the string we'll display
  memory_offset = push_eax(target, memory_offset); // (trigger index)
  memory_offset = push_u32(target, memory_offset, memory_offset_trigger_string); // (fmt)
  memory_offset = push_edx(target, memory_offset); // (buffer)
  memory_offset = call(target, memory_offset, 0x49EB80); // sprintf
  memory_offset = pop_edx(target, memory_offset); // (buffer)
  memory_offset = add_esp(target, memory_offset, 0x8);

  // Display a message
  memory_offset = push_u32(target, memory_offset, *(uint32_t*)&trigger_string_display_duration);
  memory_offset = push_edx(target, memory_offset); // (buffer)
  memory_offset = call(target, memory_offset, 0x44FCE0);
  memory_offset = add_esp(target, memory_offset, 0x8);

  // Pop the string buffer off of the stack
  memory_offset = add_esp(target, memory_offset, 0x400);

  // Jump to the real function to run the trigger
  memory_offset = jmp(target, memory_offset, 0x47CE60);

  // Install it by replacing the call destination (we'll jump to the real one)
  call(target, 0x476E80, memory_offset_trigger_code);

  return memory_offset;
}

static void patch(Target target, uint32_t memory_offset) {
#if 0
  // This is a debug feature to dump the original font textures

  dumpTextureTable(target, 0x4BF91C, 3, 0, 64, 128, "font0");
  dumpTextureTable(target, 0x4BF7E4, 3, 0, 64, 128, "font1");
  dumpTextureTable(target, 0x4BF84C, 3, 0, 64, 128, "font2");
  dumpTextureTable(target, 0x4BF8B4, 3, 0, 64, 128, "font3");
  dumpTextureTable(target, 0x4BF984, 3, 0, 64, 128, "font4");
#endif


// Start the actual patching

#if USE_PATCHED_FONTS
  memory_offset = patchTextureTable(target, memory_offset, 0x4BF91C, 0x42D745, 0x42D753, 512, 1024, "font0");
  memory_offset = patchTextureTable(target, memory_offset, 0x4BF7E4, 0x42D786, 0x42D794, 512, 1024, "font1");
  memory_offset = patchTextureTable(target, memory_offset, 0x4BF84C, 0x42D7C7, 0x42D7D5, 512, 1024, "font2");
  memory_offset = patchTextureTable(target, memory_offset, 0x4BF8B4, 0x42D808, 0x42D816, 512, 1024, "font3");
  memory_offset = patchTextureTable(target, memory_offset, 0x4BF984, 0x42D849, 0x42D857, 512, 1024, "font4");
#endif

#if USE_R100
	uint8_t upgrade_levels[7]  = {    3,    5,    5,    5,    5,    5,    5 };
#else
	uint8_t upgrade_levels[7]  = {    5,    5,    5,    5,    5,    5,    5 };
#endif
	uint8_t upgrade_healths[7] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	memory_offset = patch_network_upgrades(target, memory_offset, upgrade_levels, upgrade_healths);

#if 1
  memory_offset = patch_network_collisions(target, memory_offset);
#endif

#if 0
  uint32_t samplerate = 22050 * 2;
  uint8_t bits_per_sample = 16;
  bool stereo = true;

  memory_offset = patch_audio_stream_quality(target, memory_offset, samplerate, bits_per_sample, stereo);
#endif

#if 0
  memory_offset = patch_sprite_loader_to_load_tga(target, memory_offset);
#endif

#if USE_TRIGGER_DISPLAY
  memory_offset = patch_trigger_display(target, memory_offset);
#endif

  // Dump out the network GUID

  printf("Network GUID is: ");
  for(int i = 0; i < 16; i++) {
    printf("%02X", read8(target, 0x4AF9B0 + i));
  }
  printf("\n"); 
}

// Allocate more space, say... 4MB?
// (we use the .rsrc section, which is last in memory)
uint32_t patch_size = 4 * 1024 * 1024;

#ifndef DLL

int main(int argc, char* argv[]) {

  Target target;

  //FIXME: Retrieve this somehow
  uint32_t image_base = 0x400000;

#ifdef LOADER

  STARTUPINFO startup_info;
  memset(&startup_info, 0x00, sizeof(startup_info));
  char cmd_line[0x8000];
  strcpy(cmd_line, GetCommandLine());
  BOOL status = CreateProcess("swep1rcr.exe", cmd_line, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup_info, &target.process_information);

  printf("Status: %d\n", status);

  //FIXME: Error handling

#else

  target.f = fopen(argv[1], "rb+");
  assert(target.f != NULL);

#endif

  //FIXME: Locate this properly
  uint32_t coff_header = image_base + 212;

  // Read timestamp of binary to see which base version this is
  uint32_t timestamp = read32(target, coff_header + 4);

  //FIXME: Now set the correct pointers for this binary
  switch(timestamp) {
  case 0x3C60692C:
    break;
  default:
    printf("Unsupported version of the game, timestamp 0x%08X\n", timestamp);
    return 1;
  }

  uint32_t optional_header = coff_header + 20;
  assert(image_base == read32(target, optional_header + 28));

#ifdef LOADER

  uint32_t memory_offset = (uintptr_t)VirtualAllocEx(target.process_information.hProcess, NULL, patch_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  printf("Allocated memory at 0x%08X\n", memory_offset);

#else

  // Search for existing section
  uint32_t size_of_optional_header = read16(target, coff_header + 16);
  uint32_t section_header = optional_header + size_of_optional_header;
  uint16_t section_count = read16(target, coff_header + 2);
#if 1
  for(int i = 0; i < section_count; i++) {
    uint32_t old_section_header = section_header + i * 40;
    uint32_t n1 = read32(target, old_section_header + 0);
    uint32_t n2 = read32(target, old_section_header + 4);
    if ((n1 == *(uint32_t*)"hack") && (n2 == 0x00000000)) {
      fprintf(stderr, "This file had already been patched!\n"
                      "This tool is unable to upgrade an existing patch.\n"
                      "Please find an unmodified file.\n"
                      "Aborting.\n");

      //FIXME: Undo patches, and allow to continue

      return 1;
    }
  }
#endif

  // Get rough offset where we'll place our stuff
  fseek(target.f, 0, SEEK_END);
  uint32_t file_offset = ftell(target.f);

  // Align offset to safe bound
  fseek(target.f, file_offset, SEEK_SET);
  file_offset = (file_offset + 0xFFF) & ~0xFFF;

  // Append section data
  while(ftell(target.f) < (file_offset + patch_size)) {
    uint8_t dummy = 0x00;
    fwrite(&dummy, 1, 1, target.f);
  }

  // Select a unused memory region (after SizeOfImage) and align it
  uint32_t memory_offset = read32(target, optional_header + 56);
  memory_offset = (memory_offset + 0xFFF) & ~0xFFF;

  // Create data for new section
  uint32_t characteristics = 0;
  characteristics |= 0x20; // Code
  characteristics |= 0x40; // Initialized Data
  characteristics |= 0x20000000; // Executable
  characteristics |= 0x40000000; // Readable
  characteristics |= 0x80000000; // Writeable

  // Append a new section
  uint32_t size_of_headers = read32(target, optional_header + 60);
  uint32_t new_section_header = section_header + section_count * 40;
  assert((new_section_header + 40) <= (image_base + size_of_headers));
  write32(target, new_section_header + 0, *(uint32_t*)"hack");
  write32(target, new_section_header + 4, 0x00000000);
  write32(target, new_section_header + 8, patch_size);
  write32(target, new_section_header + 12, memory_offset);
  write32(target, new_section_header + 16, patch_size);
  write32(target, new_section_header + 20, file_offset);
  write32(target, new_section_header + 24, 0x00000000);
  write32(target, new_section_header + 28, 0x00000000);
  write32(target, new_section_header + 32, 0x00000000);
  write32(target, new_section_header + 36, characteristics);

  // Increment number of sections
  patch16_add(target, coff_header + 2, 1);

  // size of image
  write32(target, optional_header + 56, memory_offset + patch_size);

  // size of code
  patch32_add(target, optional_header + 4, patch_size);

  // size of intialized data
  patch32_add(target, optional_header + 8, patch_size);



  // Add image base
  memory_offset += image_base;

#endif

  patch(target, memory_offset);

#ifdef LOADER

  printf("Running the game\n");
  fflush(stdout);
  ResumeThread(target.process_information.hThread);

#else

  fclose(target.f);

#endif

  return 0;
}

#else

BOOL WINAPI DllMain(
  HINSTANCE hinstDLL,
  DWORD fdwReason,
  LPVOID lpReserved
) {
  return TRUE;
}

HRESULT WINAPI DirectInputCreateA(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {

  static HRESULT(WINAPI *o_DirectInputCreateA)(uint32_t, uint32_t, uint32_t, uint32_t) = NULL;
  if (o_DirectInputCreateA == NULL) {

    Target target;
    uint32_t memory_offset = (uintptr_t)VirtualAlloc(NULL, patch_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
   
    patch(target, memory_offset);

    HMODULE dll = LoadLibrary("c:/windows/system32/dinput.dll");
    o_DirectInputCreateA = (void*)GetProcAddress(dll, "DirectInputCreateA");

#if 0
    char buf[1024];
    sprintf(buf, "Hooked 0x%X", o_DirectInputCreateA);
    MessageBoxA(NULL, buf, "swe1r-patcher", 0);
#endif

  }

  return o_DirectInputCreateA(a, b, c, d);
}

#endif


