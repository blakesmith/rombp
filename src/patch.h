typedef enum rombp_patch_type {
    PATCH_TYPE_UNKNOWN = -1,
    PATCH_TYPE_IPS = 0,
} rombp_patch_type;

typedef enum rombp_patch_err {
    PATCH_OK = 0,
    PATCH_INVALID_HEADER = 1,
    PATCH_ERR_IO = 2,
} rombp_patch_err;

typedef enum rombp_hunk_iter_status {
    HUNK_ERR_IPS = -1,
    HUNK_NONE = 0,
    HUNK_DONE = 1,
    HUNK_NEXT = 2,
} rombp_hunk_iter_status;

