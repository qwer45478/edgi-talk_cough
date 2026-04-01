#include "common_storage.h"

#include <string.h>
#include <stdarg.h>
#include <time.h>

#define DBG_TAG "common.fs"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef RT_USING_DFS
#include <dfs_posix.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <dirent.h>
#endif

/* WAV file header (44 bytes) */
#pragma pack(push, 1)
typedef struct
{
    char     riff_tag[4];       /* "RIFF" */
    uint32_t file_size;         /* file size - 8 */
    char     wave_tag[4];       /* "WAVE" */
    char     fmt_tag[4];        /* "fmt " */
    uint32_t fmt_size;          /* 16 for PCM */
    uint16_t audio_format;      /* 1 = PCM */
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_tag[4];       /* "data" */
    uint32_t data_size;         /* raw data size */
} wav_header_t;
#pragma pack(pop)

static rt_bool_t s_mounted = RT_FALSE;

#ifdef RT_USING_DFS
static int ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        return RT_EOK;
    }
    return mkdir(path, 0);
}
#endif

int common_storage_init(void)
{
#ifdef RT_USING_DFS
    /* Check if SD card is mounted */
    struct stat st;
    if (stat(COMMON_STORAGE_BASE_PATH, &st) == 0)
    {
        s_mounted = RT_TRUE;
        LOG_I("storage mounted at %s", COMMON_STORAGE_BASE_PATH);
    }
    else
    {
        s_mounted = RT_FALSE;
        LOG_W("storage not mounted at %s, will retry later", COMMON_STORAGE_BASE_PATH);
    }
#endif
    LOG_I("storage service initialized");
    return RT_EOK;
}

int common_storage_ensure_dirs(void)
{
#ifdef RT_USING_DFS
    if (!s_mounted)
    {
        struct stat st;
        if (stat(COMMON_STORAGE_BASE_PATH, &st) == 0)
        {
            s_mounted = RT_TRUE;
        }
        else
        {
            return -RT_ERROR;
        }
    }

    ensure_directory(COMMON_STORAGE_LOG_DIR);
    ensure_directory(COMMON_STORAGE_AUDIO_DIR);
    ensure_directory(COMMON_STORAGE_CACHE_DIR);
    return RT_EOK;
#else
    return -RT_ENOSYS;
#endif
}

rt_bool_t common_storage_is_mounted(void)
{
    return s_mounted;
}

int common_storage_append_text(const char *path, const char *text)
{
#ifdef RT_USING_DFS
    int fd;
    ssize_t written;

    if ((path == RT_NULL) || (text == RT_NULL))
    {
        return -RT_EINVAL;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0);
    if (fd < 0)
    {
        return -RT_ERROR;
    }

    written = write(fd, text, strlen(text));
    close(fd);
    return (written >= 0) ? RT_EOK : -RT_ERROR;
#else
    RT_UNUSED(path);
    RT_UNUSED(text);
    return -RT_ENOSYS;
#endif
}

int common_storage_write_text(const char *path, const char *text)
{
#ifdef RT_USING_DFS
    int fd;
    ssize_t written;

    if ((path == RT_NULL) || (text == RT_NULL))
    {
        return -RT_EINVAL;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0)
    {
        return -RT_ERROR;
    }

    written = write(fd, text, strlen(text));
    close(fd);
    return (written >= 0) ? RT_EOK : -RT_ERROR;
#else
    RT_UNUSED(path);
    RT_UNUSED(text);
    return -RT_ENOSYS;
#endif
}

int common_storage_write_binary(const char *path, const void *data, rt_size_t size)
{
#ifdef RT_USING_DFS
    int fd;
    ssize_t written;

    if (path == RT_NULL || data == RT_NULL)
    {
        return -RT_EINVAL;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0)
    {
        return -RT_ERROR;
    }

    written = write(fd, data, size);
    close(fd);
    return (written == (ssize_t)size) ? RT_EOK : -RT_ERROR;
#else
    RT_UNUSED(path);
    RT_UNUSED(data);
    RT_UNUSED(size);
    return -RT_ENOSYS;
#endif
}

int common_storage_append_binary(const char *path, const void *data, rt_size_t size)
{
#ifdef RT_USING_DFS
    int fd;
    ssize_t written;

    if (path == RT_NULL || data == RT_NULL)
    {
        return -RT_EINVAL;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0);
    if (fd < 0)
    {
        return -RT_ERROR;
    }

    written = write(fd, data, size);
    close(fd);
    return (written == (ssize_t)size) ? RT_EOK : -RT_ERROR;
#else
    RT_UNUSED(path);
    RT_UNUSED(data);
    RT_UNUSED(size);
    return -RT_ENOSYS;
#endif
}

int common_storage_read_binary(const char *path, void *buf, rt_size_t size, rt_size_t *read_size)
{
#ifdef RT_USING_DFS
    int fd;
    ssize_t n;

    if (path == RT_NULL || buf == RT_NULL)
    {
        return -RT_EINVAL;
    }

    fd = open(path, O_RDONLY, 0);
    if (fd < 0)
    {
        return -RT_ERROR;
    }

    n = read(fd, buf, size);
    close(fd);

    if (n < 0)
    {
        return -RT_ERROR;
    }
    if (read_size != RT_NULL)
    {
        *read_size = (rt_size_t)n;
    }
    return RT_EOK;
#else
    RT_UNUSED(path);
    RT_UNUSED(buf);
    RT_UNUSED(size);
    RT_UNUSED(read_size);
    return -RT_ENOSYS;
#endif
}

int common_storage_wav_create(const char *path, rt_uint32_t sample_rate,
                              rt_uint16_t bits_per_sample, rt_uint16_t channels)
{
#ifdef RT_USING_DFS
    int fd;
    wav_header_t hdr;

    if (path == RT_NULL)
    {
        return -RT_EINVAL;
    }

    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.riff_tag, "RIFF", 4);
    memcpy(hdr.wave_tag, "WAVE", 4);
    memcpy(hdr.fmt_tag, "fmt ", 4);
    memcpy(hdr.data_tag, "data", 4);

    hdr.fmt_size        = 16;
    hdr.audio_format    = 1;   /* PCM */
    hdr.num_channels    = channels;
    hdr.sample_rate     = sample_rate;
    hdr.bits_per_sample = bits_per_sample;
    hdr.block_align     = channels * (bits_per_sample / 8);
    hdr.byte_rate       = sample_rate * hdr.block_align;
    hdr.data_size       = 0;   /* will be updated by finalize */
    hdr.file_size       = sizeof(hdr) - 8;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0)
    {
        return -RT_ERROR;
    }

    write(fd, &hdr, sizeof(hdr));
    close(fd);
    return RT_EOK;
#else
    RT_UNUSED(path);
    RT_UNUSED(sample_rate);
    RT_UNUSED(bits_per_sample);
    RT_UNUSED(channels);
    return -RT_ENOSYS;
#endif
}

int common_storage_wav_append(const char *path, const void *pcm_data, rt_size_t bytes)
{
    return common_storage_append_binary(path, pcm_data, bytes);
}

int common_storage_wav_finalize(const char *path)
{
#ifdef RT_USING_DFS
    int fd;
    struct stat st;
    uint32_t data_size;
    uint32_t file_size;

    if (path == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (stat(path, &st) != 0)
    {
        return -RT_ERROR;
    }

    data_size = (uint32_t)(st.st_size - sizeof(wav_header_t));
    file_size = (uint32_t)(st.st_size - 8);

    fd = open(path, O_WRONLY, 0);
    if (fd < 0)
    {
        return -RT_ERROR;
    }

    /* Update RIFF chunk size at offset 4 */
    lseek(fd, 4, SEEK_SET);
    write(fd, &file_size, 4);

    /* Update data chunk size at offset 40 */
    lseek(fd, 40, SEEK_SET);
    write(fd, &data_size, 4);

    close(fd);
    LOG_I("WAV finalized: %s (%lu bytes data)", path, (unsigned long)data_size);
    return RT_EOK;
#else
    RT_UNUSED(path);
    return -RT_ENOSYS;
#endif
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Storage space management — FIFO eviction of oldest audio files     */
/* ─────────────────────────────────────────────────────────────────── */
void common_storage_ensure_space(uint32_t need_bytes)
{
#ifdef RT_USING_DFS
    struct statfs sfs;
    char oldest[80];
    DIR *dir;
    struct dirent *entry;

    while (1)
    {
        if (statfs(COMMON_STORAGE_BASE_PATH, &sfs) != 0)
            break;

        uint32_t free_bytes = (uint32_t)sfs.f_bfree * (uint32_t)sfs.f_bsize;
        if (free_bytes >= need_bytes + COMMON_STORAGE_MIN_FREE)
            break;   /* enough space */

        /* Find the oldest file in cough_audio/ (lexicographic = chronological) */
        dir = opendir(COMMON_STORAGE_AUDIO_DIR);
        if (dir == RT_NULL)
            break;

        oldest[0] = '\0';
        while ((entry = readdir(dir)) != RT_NULL)
        {
            if (entry->d_type != DT_REG)
                continue;
            if (oldest[0] == '\0' || rt_strcmp(entry->d_name, oldest + rt_strlen(COMMON_STORAGE_AUDIO_DIR) + 1) < 0)
            {
                rt_snprintf(oldest, sizeof(oldest), "%s/%s",
                            COMMON_STORAGE_AUDIO_DIR, entry->d_name);
            }
        }
        closedir(dir);

        if (oldest[0] == '\0')
        {
            LOG_W("No audio files to evict, cannot free space");
            break;
        }

        LOG_I("Evicting oldest audio file: %s", oldest);
        unlink(oldest);
    }
#else
    RT_UNUSED(need_bytes);
#endif
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Event logging — daily timestamped log files                        */
/* ─────────────────────────────────────────────────────────────────── */
void common_storage_log_event(const char *fmt, ...)
{
#ifdef RT_USING_DFS
    char path[64];
    char line[200];
    int pos;
    va_list ap;
    time_t now;
    struct tm *t;

    if (!s_mounted)
        return;

    now = time(RT_NULL);
    t = localtime(&now);
    if (t == RT_NULL)
        return;

    /* File path: /sdcard/cough_log/event_YYYYMMDD.txt */
    rt_snprintf(path, sizeof(path), "%s/event_%04d%02d%02d.txt",
                COMMON_STORAGE_LOG_DIR,
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    /* Timestamp prefix */
    pos = rt_snprintf(line, sizeof(line), "[%02d:%02d:%02d] ",
                      t->tm_hour, t->tm_min, t->tm_sec);

    /* User message */
    va_start(ap, fmt);
    pos += rt_vsnprintf(line + pos, sizeof(line) - pos, fmt, ap);
    va_end(ap);

    /* Ensure newline */
    if (pos > 0 && pos < (int)(sizeof(line) - 1) && line[pos - 1] != '\n')
    {
        line[pos++] = '\n';
        line[pos] = '\0';
    }

    common_storage_append_text(path, line);
#else
    RT_UNUSED(fmt);
#endif
}

void common_storage_log_rotate(void)
{
#ifdef RT_USING_DFS
    DIR *dir;
    struct dirent *entry;
    char path[80];
    char cutoff_name[24];
    time_t now;
    struct tm *t;
    time_t cutoff_time;

    if (!s_mounted)
        return;

    now = time(RT_NULL);
    cutoff_time = now - (COMMON_STORAGE_LOG_MAX_DAYS * 24 * 3600);
    t = localtime(&cutoff_time);
    if (t == RT_NULL)
        return;

    rt_snprintf(cutoff_name, sizeof(cutoff_name), "event_%04d%02d%02d.txt",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    dir = opendir(COMMON_STORAGE_LOG_DIR);
    if (dir == RT_NULL)
        return;

    while ((entry = readdir(dir)) != RT_NULL)
    {
        if (entry->d_type != DT_REG)
            continue;
        /* Only process event_*.txt files */
        if (rt_strncmp(entry->d_name, "event_", 6) != 0)
            continue;
        /* Delete if filename sorts before the cutoff (older) */
        if (rt_strcmp(entry->d_name, cutoff_name) < 0)
        {
            rt_snprintf(path, sizeof(path), "%s/%s",
                        COMMON_STORAGE_LOG_DIR, entry->d_name);
            LOG_I("Rotating old log: %s", path);
            unlink(path);
        }
    }
    closedir(dir);
#endif
}