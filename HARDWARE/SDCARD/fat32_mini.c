#include "fat32_mini.h"
#include "sdcard.h"
#include "lcd.h"
#include <string.h>

#define SECTOR_SIZE 512
#define ATTR_DIRECTORY 0x10
#define ATTR_LONG_NAME 0x0F

static u8 sec[SECTOR_SIZE];

static u32 bytes_per_sector;
static u8  sectors_per_cluster;
static u32 reserved_sectors;
static u8  num_fats;
static u32 sectors_per_fat;
static u32 root_cluster;
static u32 fat_begin_lba;
static u32 cluster_begin_lba;
static u32 partition_lba;

static u16 rd16(const u8 *p)
{
    return (u16)p[0] | ((u16)p[1] << 8);
}

static u32 rd32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u8 read_sector(u32 lba)
{
    return SDCard_ReadBlocks(sec, lba, 1);
}

static u32 cluster_to_lba(u32 cluster)
{
    return cluster_begin_lba + (cluster - 2) * sectors_per_cluster;
}

static u8 fat_get_next(u32 cluster, u32 *next)
{
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_begin_lba + (fat_offset / SECTOR_SIZE);
    u32 ent_offset = fat_offset % SECTOR_SIZE;

    if (read_sector(fat_sector)) return 1;
    *next = rd32(sec + ent_offset) & 0x0FFFFFFF;
    return 0;
}

static void make_83_name(const char *name, char out[11])
{
    int i;
    const char *dot;

    for (i = 0; i < 11; i++) out[i] = ' ';

    dot = strchr(name, '.');
    if (dot == 0)
    {
        dot = name + strlen(name);
    }

    for (i = 0; i < 8 && name + i < dot && name[i]; i++)
    {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[i] = c;
    }

    if (*dot == '.')
    {
        dot++;
        for (i = 0; i < 3 && dot[i]; i++)
        {
            char c = dot[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + i] = c;
        }
    }
}

static u8 find_in_dir(u32 dir_cluster, const char *name, u8 want_dir, u32 *out_cluster, u32 *out_size)
{
    char target[11];
    u32 cluster = dir_cluster;
    u32 next;
    u32 s;
    int off;

    make_83_name(name, target);

    while (cluster < 0x0FFFFFF8)
    {
        for (s = 0; s < sectors_per_cluster; s++)
        {
            if (read_sector(cluster_to_lba(cluster) + s)) return 2;

            for (off = 0; off < SECTOR_SIZE; off += 32)
            {
                u8 first = sec[off];
                u8 attr  = sec[off + 11];
                u32 high, low;

                if (first == 0x00) return 3;        // 目录结束
                if (first == 0xE5) continue;        // 删除项
                if (attr == ATTR_LONG_NAME) continue;

                if (memcmp(sec + off, target, 11) == 0)
                {
                    if (want_dir && !(attr & ATTR_DIRECTORY)) return 4;
                    if (!want_dir && (attr & ATTR_DIRECTORY)) return 5;

                    high = rd16(sec + off + 20);
                    low  = rd16(sec + off + 26);
                    *out_cluster = (high << 16) | low;
                    *out_size = rd32(sec + off + 28);
                    return 0;
                }
            }
        }

        if (fat_get_next(cluster, &next)) return 6;
        cluster = next;
    }

    return 7;
}

u8 FAT32_Mount(void)
{
    u32 part_type;

    partition_lba = 0;

    /* 先读 MBR */
    if (read_sector(0)) return 1;

    if (sec[510] == 0x55 && sec[511] == 0xAA)
    {
        part_type = sec[0x1BE + 4];
        if (part_type == 0x0B || part_type == 0x0C || part_type == 0x0E)
        {
            partition_lba = rd32(sec + 0x1BE + 8);
        }
    }

    /* 读 DBR/BPB */
    if (read_sector(partition_lba)) return 2;
    if (sec[510] != 0x55 || sec[511] != 0xAA) return 3;

    bytes_per_sector    = rd16(sec + 11);
    sectors_per_cluster = sec[13];
    reserved_sectors    = rd16(sec + 14);
    num_fats            = sec[16];
    sectors_per_fat     = rd32(sec + 36);
    root_cluster        = rd32(sec + 44);

    if (bytes_per_sector != 512) return 4;
    if (sectors_per_cluster == 0) return 5;
    if (sectors_per_fat == 0) return 6;
    if (root_cluster < 2) return 7;

    fat_begin_lba     = partition_lba + reserved_sectors;
    cluster_begin_lba = partition_lba + reserved_sectors + num_fats * sectors_per_fat;

    return 0;
}

u8 FAT32_OpenFile(const char *path, u32 *first_cluster, u32 *file_size)
{
    char component[16];
    u32 dir_cluster = root_cluster;
    u32 found_cluster = 0;
    u32 found_size = 0;
    const char *p = path;
    int n;

    if (*p == '0' && *(p + 1) == ':') p += 2;
    while (*p == '/' || *p == '\\') p++;

    while (*p)
    {
        n = 0;
        while (*p && *p != '/' && *p != '\\' && n < 15)
        {
            component[n++] = *p++;
        }
        component[n] = 0;
        while (*p == '/' || *p == '\\') p++;

        if (*p)
        {
            if (find_in_dir(dir_cluster, component, 1, &found_cluster, &found_size)) return 10;
            dir_cluster = found_cluster;
        }
        else
        {
            if (find_in_dir(dir_cluster, component, 0, &found_cluster, &found_size)) return 11;
            *first_cluster = found_cluster;
            *file_size = found_size;
            return 0;
        }
    }

    return 12;
}

u8 FAT32_DisplayRGB565File(const char *path, u16 x, u16 y, u16 w, u16 h)
{
    u32 cluster;
    u32 size;
    u32 remain;
    u32 next;
    u32 s;
    u32 need_bytes = (u32)w * h * 2;

    if (FAT32_OpenFile(path, &cluster, &size)) return 1;
    if (size < need_bytes) return 2;

    remain = need_bytes;
    LCD_Set_Window(x, y, w, h);

    while (cluster < 0x0FFFFFF8 && remain)
    {
        for (s = 0; s < sectors_per_cluster && remain; s++)
        {
            u32 bytes = remain > SECTOR_SIZE ? SECTOR_SIZE : remain;
            if (read_sector(cluster_to_lba(cluster) + s)) return 3;
            LCD_WriteBytes(sec, bytes);
            remain -= bytes;
        }

        if (remain)
        {
            if (fat_get_next(cluster, &next)) return 4;
            cluster = next;
        }
    }

    return remain ? 5 : 0;
}
