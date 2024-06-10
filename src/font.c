#include "font.h"
#include "defines.h"

#include <endian.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PACKED __attribute__((packed))

typedef u8 tag_t[4];
typedef struct {
    u16 major;
    u16 minor;
} PACKED version16dot16_t;
typedef struct {
    u16 integer;
    u16 fraction;
} PACKED fixed_t;

struct table_record {
    tag_t tag;
    u32 checksum;
    u32 offset;
    u32 length;
} PACKED;

struct table_directory {
    u32 sfnt_version;
    u16 num_tables;
    u16 search_range;
    u16 entry_selector;
    u16 range_shift;
    struct table_record table_records[];
} PACKED;

struct otf_reader {
    u8 *buffer;
    u64 buf_len;
    struct table_directory *directory;
};

static void otf_reader_init(struct otf_reader *r, u8 *buffer, u64 length) {
    *r = (struct otf_reader){0};
    r->buffer = buffer;
    r->buf_len = length;

    struct table_directory *temp = (struct table_directory *)buffer;

    r->directory = malloc(sizeof(*r->directory) +
                          sizeof(r->directory->table_records[0]) * be16toh(temp->num_tables));
    r->directory->sfnt_version = be32toh(temp->sfnt_version);
    r->directory->num_tables = be16toh(temp->num_tables);
    r->directory->search_range = be16toh(temp->search_range);
    r->directory->entry_selector = be16toh(temp->entry_selector);
    r->directory->range_shift = be16toh(temp->range_shift);

    u64 offset = sizeof(*r->directory);
    for (u16 i = 0; i < r->directory->num_tables; i++) {
        struct table_record *current_record = &r->directory->table_records[i];
        struct table_record *read_record = (struct table_record *)(buffer + offset);
        memcpy(current_record->tag, read_record->tag, sizeof(current_record->tag));
        current_record->checksum = be32toh(read_record->checksum);
        current_record->offset = be32toh(read_record->offset);
        current_record->length = be32toh(read_record->length);
        offset += sizeof(struct table_record);
    }
}

static void otf_reader_destroy(struct otf_reader *r) { free(r->directory); }

static void parse_version16dot16(version16dot16_t version, u16 *major, u16 *minor) {
    *major = be16toh(version.major);
    *minor = be16toh(version.minor);
}

static u32 otf_reader_get_table_offset(const struct otf_reader *r, tag_t tag) {
    for (u16 i = 0; i < r->directory->num_tables; i++) {
        if (memcmp(tag, r->directory->table_records[i].tag, sizeof(tag_t)) == 0) {
            return r->directory->table_records[i].offset;
        }
    }
    return 0;
}

struct maxp_table {
    version16dot16_t version;
    u16 num_glyphs;
} PACKED;

struct maxp_table_data {
    u16 version_major;
    u16 version_minor;
    u16 num_glyphs;
};

static void otf_reader_read_maxp_table(struct otf_reader *r, struct maxp_table_data *data) {
    *data = (struct maxp_table_data){0};

    u32 table_offset = otf_reader_get_table_offset(r, (tag_t){'m', 'a', 'x', 'p'});

    struct maxp_table *read_table = (struct maxp_table *)(r->buffer + table_offset);
    parse_version16dot16(read_table->version, &data->version_major, &data->version_minor);
    data->num_glyphs = be16toh(read_table->num_glyphs);
}

enum index_to_loc_format {
    OFFSET16 = 0,
    OFFSET32 = 1,
};

struct head_table {
    u16 major_version;
    u16 minor_version;
    fixed_t font_revision;
    u32 checksum_adjustment;
    u32 magic_number;
    u16 flags;
    u16 units_per_em;
    i64 created;
    i64 modified;
    i16 xmin;
    i16 ymin;
    i16 xmax;
    i16 ymax;
    u16 mac_style;
    u16 lowest_rec_ppem;
    i16 font_direction_hint;
    i16 index_to_loc_format;
    i16 glyph_data_format;
} PACKED;

STATIC_ASSERT(sizeof(struct head_table) == 54, "");

struct head_table_data {
    u16 units_per_em;
    i16 xmin;
    i16 ymin;
    i16 xmax;
    i16 ymax;
    u16 style;
    u16 lowest_recommended_ppem;
    enum index_to_loc_format index_to_loc_format;
};

#define HEAD_TABLE_MAGIC_NUMBER 0x5f0f3cf5

static b8 otf_reader_read_head_table(struct otf_reader *r, struct head_table_data *data) {
    *data = (struct head_table_data){0};

    u32 table_offset = otf_reader_get_table_offset(r, (tag_t){'h', 'e', 'a', 'd'});

    struct head_table *table = (struct head_table *)(r->buffer + table_offset);

    if (be16toh(table->major_version) != 1) {
        return false;
    }
    if (be16toh(table->minor_version) != 0) {
        return false;
    }
    if (be32toh(table->magic_number) != HEAD_TABLE_MAGIC_NUMBER) {
        return false;
    }
    data->index_to_loc_format = be16toh(table->index_to_loc_format);
    if (data->index_to_loc_format != 0 && data->index_to_loc_format != 1) {
        return false;
    }

    data->units_per_em = be16toh(table->units_per_em);
    data->xmin = be16toh(table->xmin);
    data->ymin = be16toh(table->ymin);
    data->xmax = be16toh(table->xmax);
    data->ymax = be16toh(table->ymax);
    data->style = be16toh(table->mac_style);
    data->lowest_recommended_ppem = be16toh(table->lowest_rec_ppem);

    return true;
}

struct loca_table_data {
    const void *slice;
    u32 table_len;
    enum index_to_loc_format format;
};

static void otf_reader_read_loca_table(struct otf_reader *r,
                                       const struct maxp_table_data *maxp,
                                       const struct head_table_data *head,
                                       struct loca_table_data *data) {
    *data = (struct loca_table_data){0};

    data->slice = r->buffer + otf_reader_get_table_offset(r, (tag_t){'l', 'o', 'c', 'a'});
    data->format = head->index_to_loc_format;
    data->table_len = maxp->num_glyphs + 1;
}

static u32 loca_table_get_glyph_offset(const struct loca_table_data *loca_table, u32 glyph_id) {
    switch (loca_table->format) {
    case OFFSET16:
        return ((u32)be16toh(*(u16 *)((u64)loca_table->slice + glyph_id * 2))) * 2;
    case OFFSET32:
        return (be32toh(*(u32 *)((u64)loca_table->slice + glyph_id * 4)));
    }
    return 0;
}

struct encoding_record {
    u16 platform_id;
    u16 encoding_id;
    u32 subtable_offset;
} PACKED;

struct cmap_header {
    u16 version;
    u16 num_tables;
    struct encoding_record encoding_records[];
} PACKED;

enum cmap_platform {
    CMAP_PLATFORM_UNICODE = 0,
    CMAP_PLATFORM_MACINTOSH = 1,
    CMAP_PLATFORM_WINDOWS = 3,
    CMAP_PLATFORM_CUSTOM = 4,
};

enum cmap_subtable_format {
    CMAP_FORMAT_BYTE_ENCODING = 0,
    CMAP_FORMAT_HIGH_BYTE = 2,
    CMAP_FORMAT_SEGMENT_TO_DELTA = 4,
    CMAP_FORMAT_TRIMMED_TABLE = 6,
    CMAP_FORMAT_MIXED_16_AND_32 = 8,
    CMAP_FORMAT_TRIMMED_ARRAY = 10,
    CMAP_FORMAT_SEGMENTED_COVERAGE = 12,
    CMAP_FORMAT_MANY_TO_ONE_RANGE = 13,
    CMAP_FORMAT_UNICODE_VARIATION_SEQUENCES = 14,
};

enum cmap_unicode_encoding {
    CMAP_UNICODE_ENCODING_DEPRECATED_UNICODE_1_0 = 0,
    CMAP_UNICODE_ENCODING_DEPRECATED_UNICODE_1_1 = 1,
    CMAP_UNICODE_ENCODING_DEPRECATED_ISO_10646 = 2,
    CMAP_UNICODE_ENCODING_UNICODE_2_0_BMP_ONLY = 3,
    CMAP_UNICODE_ENCODING_UNICODE_2_0_FULL_REPERTOIRE = 4,
    CMAP_UNICODE_ENCODING_UNICODE_VARIATION_SEQUENCES = 5,
    CMAP_UNICODE_ENCODING_UNICODE_FULL_REPERTOIRE = 6,
};

enum cmap_windows_encoding {
    CMAP_WINDOWS_ENCODING_UNICODE_BMP = 1,
    CMAP_WINDOWS_ENCODING_UNICODE_FULL_REPERTOIRE = 10,
};
struct encoding_record_data {
    u16 platform_id;
    u16 encoding_id;
    u32 subtable_offset;
};

struct cmap_table_data {
    const void *table_start_ref;
    u16 num_tables;
    struct encoding_record_data *encoding_records;
};

static void otf_reader_read_cmap_table(const struct otf_reader *r, struct cmap_table_data *data) {
    *data = (struct cmap_table_data){0};

    data->table_start_ref = r->buffer + otf_reader_get_table_offset(r, (tag_t){'c', 'm', 'a', 'p'});
    const struct cmap_header *table = data->table_start_ref;

    data->num_tables = be16toh(table->num_tables);
    data->encoding_records = calloc(data->num_tables, sizeof(*data->encoding_records));
    for (u16 i = 0; i < data->num_tables; i++) {
        data->encoding_records[i].platform_id = be16toh(table->encoding_records[i].platform_id);
        data->encoding_records[i].encoding_id = be16toh(table->encoding_records[i].encoding_id);
        data->encoding_records[i].subtable_offset =
            be32toh(table->encoding_records[i].subtable_offset);
    }
}

struct cmap_subtable_0 {
    u16 format;
    u16 length;
    u16 language;
    u8 glyph_id_array[256];
} PACKED;

struct cmap_subtable_4_fixed_fields {
    u16 format;
    u16 length;
    u16 language;
    u16 seg_count_x2;
    u16 search_range;
    u16 entry_selector;
    u16 range_shift;
} PACKED;

struct cmap_subtable_6 {
    u16 format;
    u16 length;
    u16 language;
    u16 first_code;
    u16 entry_count;
    u16 glyph_id_array[];
} PACKED;

static u32 cmap_get_glyph_id_for_code_point_subtable_6(const struct cmap_table_data *cmap,
                                                       u16 table_index,
                                                       u32 code_point) {
    // TODO: assert here
    if (table_index >= cmap->num_tables) {
        exit(1);
    }

    void *subtable_start = (void *)((u64)cmap->table_start_ref +
                                    (u64)cmap->encoding_records[table_index].subtable_offset);
    // TODO: assert here
    if (be16toh(*(u16 *)subtable_start) != 6) {
        exit(1);
    }

    struct cmap_subtable_6 *subtable = subtable_start;
    u32 first_code = be16toh(subtable->first_code);
    if (code_point < first_code)
        return 0;

    u32 entry_count = be16toh(subtable->entry_count);
    u32 code_offset = code_point - first_code;
    if (code_offset >= entry_count)
        return 0;

    return be16toh(subtable->glyph_id_array[code_offset]);
}

struct cmap_sequential_map_group {
    u32 start_char_code;
    u32 end_char_code;
    u32 start_glyph_id;
} PACKED;

struct cmap_subtable_8 {
    u16 format;
    u16 reserved;
    u32 length;
    u32 language;
    u8 is32[8192];
    u32 num_groups;
    struct cmap_sequential_map_group groups[];
} PACKED;

struct cmap_subtable_10 {
    u16 format;
    u16 reserved;
    u32 length;
    u32 language;
    u32 start_char_code;
    u32 num_chars;
    u16 glyph_id_array[];
} PACKED;

struct cmap_subtable_12 {
    u16 format;
    u16 reserved;
    u32 length;
    u32 language;
    u32 num_groups;
    struct cmap_sequential_map_group groups[];
} PACKED;

struct cmap_constant_map_group {
    u32 start_char_code;
    u32 end_char_code;
    u32 glyph_id;
} PACKED;

struct cmap_subtable_13 {
    u16 format;
    u16 reserved;
    u32 length;
    u32 language;
    u32 num_groups;
    struct cmap_constant_map_group groups[];
} PACKED;

static void cmap_table_destroy(struct cmap_table_data *table) {
    free(table->encoding_records);
    table->encoding_records = NULL;
}

struct glyph_header {
    i16 number_of_contours;
    i16 xmin;
    i16 ymin;
    i16 xmax;
    i16 ymax;
} PACKED;

enum simple_glyph_flag {
    ON_CURVE_POINT = 0x01,
    X_SHORT_VECTOR = 0x02,
    Y_SHORT_VECTOR = 0x04,
    REPEAT_FLAG = 0x08,
    X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR = 0x10,
    Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR = 0x20,
    OVERLAP_SIMPLE = 0x40,

    X_MASK = 0x12,
    Y_MASK = 0x24,

    X_LONG_VECTOR = 0x00,
    Y_LONG_VECTOR = 0x00,
    X_NEGATIVE_SHORT_VECTOR = 0x02,
    Y_NEGATIVE_SHORT_VECTOR = 0x04,
    X_POSITIVE_SHORT_VECTOR = 0x12,
    Y_POSITIVE_SHORT_VECTOR = 0x24,
};

struct glyph_point {
    b8 on_curve;
    vec2s point;
};

struct glyph_point_iterator {
    void *glyph_slice;
    u16 points_remaining;
    vec2s last_point;
    u8 current_flag;
    u32 flags_remaining;
    u32 flags_offset;
    u32 x_offset;
    u32 y_offset;
};

static bool glyph_point_iterator_next(struct glyph_point_iterator *iterator,
                                      struct glyph_point *item) {
    if (iterator->points_remaining == 0) {
        return false;
    }
    if (iterator->flags_remaining > 0) {
        iterator->flags_remaining--;
    } else {
        iterator->current_flag = *(u8 *)((u64)iterator->glyph_slice + iterator->flags_offset++);
        if (iterator->current_flag & (u8)REPEAT_FLAG) {
            iterator->flags_remaining =
                *(u8 *)((u64)iterator->glyph_slice + iterator->flags_offset++);
        }
    }
    switch (iterator->current_flag & X_MASK) {
    case (u8)X_LONG_VECTOR:
        iterator->last_point.x =
            iterator->last_point.x +
            (i16)be16toh(*(i16 *)((u64)iterator->glyph_slice + iterator->x_offset));
        iterator->x_offset += sizeof(i16);
        break;
    case (u8)X_NEGATIVE_SHORT_VECTOR:
        iterator->last_point.x =
            iterator->last_point.x - *(u8 *)((u64)iterator->glyph_slice + iterator->x_offset++);
        break;
    case (u8)X_POSITIVE_SHORT_VECTOR:
        iterator->last_point.x =
            iterator->last_point.x + *(u8 *)((u64)iterator->glyph_slice + iterator->x_offset++);
        break;
    default:
        break;
    }
    switch (iterator->current_flag & Y_MASK) {
    case (u8)Y_LONG_VECTOR:
        iterator->last_point.y =
            iterator->last_point.y +
            (i16)be16toh(*(i16 *)((u64)iterator->glyph_slice + iterator->y_offset));
        iterator->y_offset += sizeof(i16);
        break;
    case (u8)Y_NEGATIVE_SHORT_VECTOR:
        iterator->last_point.y =
            iterator->last_point.y - *(u8 *)((u64)iterator->glyph_slice + iterator->y_offset++);
        break;
    case (u8)Y_POSITIVE_SHORT_VECTOR:
        iterator->last_point.y =
            iterator->last_point.y + *(u8 *)((u64)iterator->glyph_slice + iterator->y_offset++);
        break;
    default:
        break;
    }
    iterator->points_remaining--;
    item->on_curve = (iterator->current_flag & ON_CURVE_POINT) != 0;
    item->point = iterator->last_point;
    return true;
}

static struct glyph_point_iterator otf_reader_read_glyf(struct otf_reader *r,
                                                        const struct loca_table_data *loca,
                                                        u32 glyph_id) {
    void *glyf_table = r->buffer + otf_reader_get_table_offset(r, (tag_t){'g', 'l', 'y', 'f'});
    struct glyph_header *glyph_header =
        (void *)((u64)glyf_table + loca_table_get_glyph_offset(loca, glyph_id));
    i16 number_of_contours = be16toh(glyph_header->number_of_contours);
    printf("\tglyph_id %d: number_of_contours = %d\n", glyph_id, number_of_contours);

    if (number_of_contours <= 0)
        exit(1);

    void *glyph_table = (void *)((u64)glyph_header + sizeof(struct glyph_header));

    u16 end_points_of_contours[number_of_contours];
    for (u16 i = 0; i < number_of_contours; i++) {
        end_points_of_contours[i] = be16toh(*(u16 *)((u64)glyph_table + i * sizeof(u16)));
    }
    u16 point_count = end_points_of_contours[number_of_contours - 1] + 1;

    u16 instruction_count = be16toh(*(u16 *)((u64)glyph_table + number_of_contours * sizeof(u16)));

    u32 flags_offset = number_of_contours * sizeof(u16) + sizeof(u16) + instruction_count;

    u32 flags_size = 0;
    u32 x_size = 0;
    u32 repeat_count;
    u16 num_points = point_count;
    while (num_points > 0) {
        u8 flag = (*(u8 *)((u64)glyph_table + flags_offset + flags_size));
        if (flag & (u8)REPEAT_FLAG) {
            flags_size++;
            repeat_count = (*(u8 *)((u64)glyph_table + flags_offset + flags_size)) + 1;

        } else {
            repeat_count = 1;
        }
        flags_size++;

        switch (flag & (u8)X_MASK) {
        case (u8)X_LONG_VECTOR:
            x_size += repeat_count * sizeof(i16);
            break;
        case (u8)X_NEGATIVE_SHORT_VECTOR:
        case (u8)X_POSITIVE_SHORT_VECTOR:
            x_size += repeat_count;
            break;
        default:
            break;
        }
        num_points -= repeat_count;
    }

    u32 x_offset = flags_offset + flags_size;
    u32 y_offset = x_offset + x_size;

    struct glyph_point_iterator iterator = {
        .glyph_slice = glyph_table,
        .points_remaining = point_count,
        .flags_offset = flags_offset,
        .x_offset = x_offset,
        .y_offset = y_offset,
    };

    return iterator;
}

void load_font(const char *file_name, struct font *font) {
    (void)font;

    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        fprintf(stderr, "failed to open file: %s\n", file_name);
        return;
    }

    fseek(fp, 0, SEEK_END);
    u64 file_size = ftell(fp);
    rewind(fp);

    u8 buffer[file_size];
    fread(buffer, file_size, 1, fp);

    struct otf_reader reader;
    otf_reader_init(&reader, buffer, file_size);

    printf("%x\n", reader.directory->sfnt_version);
    for (u32 i = 0; i < reader.directory->num_tables; i++) {
        printf("%.4s @ %d\n",
               reader.directory->table_records[i].tag,
               reader.directory->table_records[i].offset);
    }

    struct maxp_table_data maxp_table = {0};
    otf_reader_read_maxp_table(&reader, &maxp_table);

    printf("maxp table:\n"
           "\tversion: %u.%u\n"
           "\tnumGlyphs: %u\n",
           maxp_table.version_major,
           maxp_table.version_minor,
           maxp_table.num_glyphs);

    struct head_table_data head_table = {0};
    otf_reader_read_head_table(&reader, &head_table);
    printf("head table:\n"
           "\tunits_per_em: %d\n"
           "\txmin: %d\n"
           "\tymin: %d\n"
           "\txmax: %d\n"
           "\tymax: %d\n"
           "\tstyle: %d\n"
           "\tlowest_recommended_ppem: %d\n"
           "\tindex_to_loc_format: %d\n",
           head_table.units_per_em,
           head_table.xmin,
           head_table.ymin,
           head_table.xmax,
           head_table.ymax,
           head_table.style,
           head_table.lowest_recommended_ppem,
           head_table.index_to_loc_format);

    struct loca_table_data loca_table = {0};
    otf_reader_read_loca_table(&reader, &maxp_table, &head_table, &loca_table);

    struct cmap_table_data cmap_table = {0};
    otf_reader_read_cmap_table(&reader, &cmap_table);

    for (u16 i = 0; i < cmap_table.num_tables; i++) {
        u16 *format = (u16 *)((u64)cmap_table.table_start_ref +
                              (u64)cmap_table.encoding_records[i].subtable_offset);
        printf("subtable %d: format = %d\n", i, be16toh(*format));
        if (be16toh(*format) == 6) {
            for (u32 c = 'A'; c <= 'Z'; c++) {
                u32 glyph_id = cmap_get_glyph_id_for_code_point_subtable_6(&cmap_table, i, c);
                printf("\t'%c' = %d\n", c, glyph_id);
            }
            for (u32 c = 'a'; c <= 'z'; c++) {
                u32 glyph_id = cmap_get_glyph_id_for_code_point_subtable_6(&cmap_table, i, c);
                printf("\t'%c' = %d\n", c, glyph_id);
                if (c == 'a') {
                    struct glyph_point_iterator iterator =
                        otf_reader_read_glyf(&reader, &loca_table, glyph_id);
                    struct glyph_point item;
                    while (glyph_point_iterator_next(&iterator, &item)) {
                        printf("\tpoints:\n");
                        printf("\t\t[%f, %f]: %s\n",
                               item.point.x,
                               item.point.y,
                               item.on_curve ? "true" : "false");
                    }
                }
            }
        }
    }

    cmap_table_destroy(&cmap_table);
    otf_reader_destroy(&reader);
    fclose(fp);
}
