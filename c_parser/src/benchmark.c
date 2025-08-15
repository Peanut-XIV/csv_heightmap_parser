#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__APPLE__) || defined(__LINUX__)
#include <dirent.h>
#include <stddef.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/mman.h>
#elif defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include "../include/win_err_status_numbers.h"
#endif

#include "../include/file_identificator.h"
#include "../include/arg_parse.h"
#include "../include/buffer_util.h"
#include "../include/utils.h"

#define SMALL_ERR_MSG_SIZE 100 // Arbitrary value
#define PARSING_ERR_LIMIT 5

#define USAGE "Usage: benchmark [config path]" ENDL

#define die(e_msg, ex_no) _die(e_msg, ex_no, USAGE)

/*  Nomenclature and expectations of the row structure of input csv files.
 *  The separator character is only the comma ',' for now.
 *
 *  ┌───────────────────────┬───────────┬───────────────────────┬─────────────┐
 *  │   min<=N<=max bytes   │  1 byte   │   min<=N<=max bytes   │  1-2 bytes  │
 *  ├───────────────────────┼───────────┼───────────────────────┼─────────────┤
 *  │ 3 . 1 4 1 5 9 2 6 5 3 │     ,     │ 3 . 1 4 1 5 9 2 6 5 3 │ \r\n or \n  │
 *  ├───────────────────────┼───────────┼───────────────────────┼─────────────┤
 *  │         field         │ separator │         field         │ end of line │
 *  ├───────────────────────┴───────────┼───────────────────────┴─────────────┤
 *  │              Stride               │              Stride                 │
 *  ├───────────────────────────────────┴─────────────────────────────────────┤
 *  │                                  Row                                    │
 *  └─────────────────────────────────────────────────────────────────────────┘
 *
 *  Obviously, the field, stride and row sizes are not fixed because of the
 *  field formatting.
 *
 *  The output format is similar, but with fixed width fields, for easier
 *  parsing in the future.
 */

// ================================= GLOBALS ==================================

#ifdef _WIN32
FILE *input_fp = NULL;
#else
static int input_fd = -1;
#endif
static void* comp_buff_ptr = NULL;

// ============================= ATEXIT FUNCTIONS =============================
#ifdef _WIN32
void close_input_fp(void){
	fclose(input_fp);
}
#else
void close_input_fd(void){
	close(input_fd);
}
#endif

void free_comp_buff(void){
	free(comp_buff_ptr);
}

// ================================= THE REST =================================
#ifdef _WIN32
void print_file_attributes(ULONG attrs) {
	if (attrs & FILE_ATTRIBUTE_READONLY) printf("FILE_ATTRIBUTE_READONLY" ENDL);
	if (attrs & FILE_ATTRIBUTE_HIDDEN) printf("FILE_ATTRIBUTE_HIDDEN" ENDL);
	if (attrs & FILE_ATTRIBUTE_SYSTEM) printf("FILE_ATTRIBUTE_SYSTEM" ENDL);
	if (attrs & FILE_ATTRIBUTE_DIRECTORY) printf("FILE_ATTRIBUTE_DIRECTORY" ENDL);
	if (attrs & FILE_ATTRIBUTE_ARCHIVE) printf("FILE_ATTRIBUTE_ARCHIVE" ENDL);
	if (attrs & FILE_ATTRIBUTE_DEVICE) printf("FILE_ATTRIBUTE_DEVICE" ENDL);
	if (attrs & FILE_ATTRIBUTE_NORMAL) printf("FILE_ATTRIBUTE_NORMAL" ENDL);
	if (attrs & FILE_ATTRIBUTE_TEMPORARY) printf("FILE_ATTRIBUTE_TEMPORARY" ENDL);
	if (attrs & FILE_ATTRIBUTE_SPARSE_FILE) printf("FILE_ATTRIBUTE_SPARSE_FILE" ENDL);
	if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) printf("FILE_ATTRIBUTE_REPARSE_POINT" ENDL);
	if (attrs & FILE_ATTRIBUTE_COMPRESSED) printf("FILE_ATTRIBUTE_COMPRESSED" ENDL);
	if (attrs & FILE_ATTRIBUTE_OFFLINE) printf("FILE_ATTRIBUTE_OFFLINE" ENDL);
	if (attrs & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) printf("FILE_ATTRIBUTE_NOT_CONTENT_INDEXED" ENDL);
	if (attrs & FILE_ATTRIBUTE_ENCRYPTED) printf("FILE_ATTRIBUTE_ENCRYPTED" ENDL);
	if (attrs & FILE_ATTRIBUTE_INTEGRITY_STREAM) printf("FILE_ATTRIBUTE_INTEGRITY_STREAM" ENDL);
	if (attrs & FILE_ATTRIBUTE_VIRTUAL) printf("FILE_ATTRIBUTE_VIRTUAL" ENDL);
	if (attrs & FILE_ATTRIBUTE_NO_SCRUB_DATA) printf("FILE_ATTRIBUTE_NO_SCRUB_DATA" ENDL);
	if (attrs & FILE_ATTRIBUTE_EA) printf("FILE_ATTRIBUTE_EA" ENDL);
	if (attrs & FILE_ATTRIBUTE_PINNED) printf("FILE_ATTRIBUTE_PINNED" ENDL);
	if (attrs & FILE_ATTRIBUTE_UNPINNED) printf("FILE_ATTRIBUTE_UNPINNED" ENDL);
	if (attrs & FILE_ATTRIBUTE_RECALL_ON_OPEN) printf("FILE_ATTRIBUTE_RECALL_ON_OPEN" ENDL);
	if (attrs & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS) printf("FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS" ENDL);
}
#endif

void specify_os_error_and_exit(void){
	/*
	 * Errno diagnostic and exit code
	 * after read call with read only `RDONLY` flag.
	*/
	switch (errno) {
		case EACCES:
			die("Access to source file not permitted", EX_NOINPUT);
			break; // useless statement since die() exits
		case EAGAIN:
		case ENXIO:
		case EOPNOTSUPP:
			die("Unexpected source filetype", EX_NOINPUT);
			break;
		case EISDIR:
			die("source path is a directory, not a file", EX_NOINPUT);
			break;
		case ELOOP:
			die("too many symlinks in source path", EX_NOINPUT);
			break;
		case ENAMETOOLONG:
			die("source path too long", EX_NOINPUT);
			break;
		case EBADF:
		case ENOENT:
			die("source file not found", EX_NOINPUT);
			break;
		case 0:
			break;
		default:
			die("Unexpected code path", EX_OSERR);
	}
	// none of the cases matched with our errno
	// default message formatting
	char msg[ERR_MSG_SIZE];
	int errval = errno;

	int printed_length = snprintf(
		msg, ERR_MSG_SIZE,
		"Unexpected syscall error n°%d: %s",
		errval, strerror(errval)
	);

	if (printed_length > ERR_MSG_SIZE){
		printf("unexpectedly long error message..." ENDL);
	}

	die(msg, EX_SOFTWARE);
}

void print_size_info(uint64_t bytes){
	uint64_t total_count = bytes;
	const char log2_1024 = 10;

	uint64_t B = total_count % 1024;
	total_count <<= log2_1024;
	uint64_t KiB = (total_count) % 1024;
	total_count <<= log2_1024;
	uint64_t MiB = (total_count) % 1024;
	total_count <<= log2_1024;
	uint64_t GiB = (total_count) % 1024;
	total_count <<= log2_1024;
	uint64_t TiB = (total_count) % 1024;

	printf("Object is of size : %lli bytes" ENDL, (long long int) bytes);
	printf("or %llu TiB, %llu GiB, %llu MiB, %llu KiB & %llu bytes." ENDL,
		   TiB, GiB, MiB, KiB, B);
}

Eol_flag Check_input_flags(Eol_flag from_config, Eol_flag as_detected) {
	/*+-------------+-------+-------+-------+
	 *|             |      as_detected      |
	 *|-------------|-------+-------+-------|
	 *| from_config | UNIX  |  DOS  | AUTO  |
	 *|-------------|-------|-------|-------|
	 *|    UNIX     |   U   |   U*  |   U*  |
	 *|-------------|-------|-------|-------|
	 *|    DOS      |   D*  |   D   |   D*  |
	 *|-------------|-------|-------|-------|
	 *|    AUTO     |   U   |   D   |   A*  |
	 *+-------------+-------+-------+-------+
	 * `*` means a user warning is printed as to show discrepancies between
	 * config and provided file
	 * For `as_detected`, `AUTO` means that no end of line was detected,
	 * explaining the systematic warning when it shows up.
	 */
	if (from_config == EOL_AUTO && as_detected == EOL_AUTO) {
		printf("No eol type could be identified during detection, and no "
			   "fallback option was provided" ENDL);
		return EOL_AUTO;
	}
	if (as_detected == EOL_AUTO) {
		printf("WARNING: no end of line was detected, falling back to configuration" ENDL);
	} else if (from_config != as_detected && from_config != EOL_AUTO) {
		printf("WARNING: The end-of-line marker specified in the configuration does"
			   " not match\nwith the one detected. Falling back to configuration" ENDL);
	}
	return (from_config == EOL_AUTO) ? as_detected : from_config;
}

int parse_readbuffer_line(char* start, char** end, ParserConfig* conf, float* outptr) {
	if (start == NULL) {
		return PARSING_ERR_LIMIT;
	}
	char* current = start;
	char* prev = start;
	char* fend = start;

	char errcount = 0;
	ptrdiff_t offset;

	const char endl = conf->line.eol == EOL_UNIX ? '\n':'\r';
	const short max_sep_dist = conf->field.max + (endl=='\n' ? 1 : 2);

	for (size_t count = 0 ; (count < conf->line.field_count) ; count++) {
		fend = current;

		errno = 0;
		outptr[count] = strtof(current, &fend);
		int errval = errno;

		if (errval) errcount += 1;

		if (fend == current) {
			// strtof failed because of a missing field
			fend = memchr(current, ',', max_sep_dist); // TODO: replace by a `sep` variable
			if (fend == NULL) {
				errcount += PARSING_ERR_LIMIT;
				break;
			}
		}

		offset = fend - current;
		if (
			((int32_t) offset < conf->field.min)
			|| ((int32_t) offset > conf->field.max)
			|| errno
		) { //write down error in flag
			errcount += 1;
		}

		prev = fend;
		current = fend + 1;

		if ((*prev == endl) || (errcount >= PARSING_ERR_LIMIT)) break;
	}
	if (fend == NULL) return PARSING_ERR_LIMIT;

	if (*fend == '\n') fend++;
	*end = fend;

	return errcount;
}

int parse_chunk(char** start, Config* conf, ParserConfig* pconf, float** outptr) {
	//	should provide:
	//		ptr to end of current readchunk
	//		ptr to end of current compute chunk
	//		error status
	char *read_start = *start;
	char *read_end;
	float *current_out = *outptr;
	int UNRECOVERABLE = 0;
	int errcount = 0;

	for (int row = 0; row < (conf->tile_height * 2); row ++) {
		errcount += parse_readbuffer_line(read_start, &read_end, pconf, current_out);
		if (errcount >= PARSING_ERR_LIMIT) {
			printf("too many errors" ENDL);
			UNRECOVERABLE = 1;
			break;
		}
		read_start = read_end;
		current_out += (pconf->line.field_count * 2);
	}
	*start = read_start;
	*outptr = current_out;
	return UNRECOVERABLE;
}

int init_RowLayout(RowLayout *rl, const RowInfo *ri, const Config *cf) {
	Eol_flag eol = Check_input_flags(cf->eol_flag, ri->eol_flag);
	if (eol == EOL_AUTO) return 1;

	rl->eol_size = (eol == EOL_UNIX) ? 1 : 2;
	rl->sep_size = 1;
	rl->max_field_size = cf->max_field_size;
	rl->min_field_size = cf->min_field_size;
	rl->field_count = ri->count;
	rl->max_size = ((int64_t) cf->max_field_size + rl->sep_size) * ri->count - rl->sep_size + rl->eol_size;
	return 0;
}

int init_CompBuffer(CompBuffer *cb, const RowLayout *row_lo, const Config *cf) {
	init_CompBufferStruct(cb, row_lo, cf);
	cb->start = (float *) malloc(cb->bytesize);
	if (cb->start == NULL) {
		printf("couldn't allocate memory for computation buffer" ENDL);
		print_size_info(cb->bytesize);
		return 1;
	} else {
		comp_buff_ptr = cb->start;
		if (atexit(free_comp_buff)) die("could not set compute buffer auto exit", EX_OSERR);
		return 0;
	}
}

int init_WriteBufferStruct(WriteBuffer* wb, ProcValBuffer* pvb, Config* conf){
	//sizes of different elements
	char sep = 1;
	int stride = conf->output_field_size + sep;
	char eol = conf->eol_flag == EOL_UNIX ? 1 : 2;

	// we don't malloc the whole buffer
	wb->buffer = NULL;

	// but we malloc the array of FileBuffers (not actual buffers)
	div_t qr = div(pvb->row_length, conf->tile_width);
	int file_count = qr.quot + (qr.rem ? 1 : 0);
	wb->file_buffers = (FileBuffer *) malloc(file_count * sizeof(FileBuffer));
	if (wb->file_buffers == NULL) return 1;

	//some data
	wb->file_buffer_count = file_count;
	wb->sep_size = sep;
	wb->field_size = conf->output_field_size;
	wb->eol_size = eol;

	//initializing the structs inside
	wb->bytesize = 0;
	for (int i = 0; i < file_count; i++) {
		FileBuffer *fb = wb->file_buffers + i;
		fb->buffer = NULL;
		fb->row_length = (i != file_count - 1) ? conf->tile_width : qr.rem;
		fb->row_size = fb->row_length * stride - sep + eol;
		fb->bytesize = (int64_t) fb->row_size * pvb->row_count;
		wb->bytesize += fb->bytesize;
	}
	if (wb->file_buffers == NULL) return 1;
	else return 0;
}

void asign_filebuffers(WriteBuffer *wrb) {
	char* buff_start = wrb->buffer;
	FileBuffer *start = wrb->file_buffers;
	FileBuffer *end = start + wrb->file_buffer_count;
	
	for (FileBuffer *fb = start; fb < end; fb++){
		fb->buffer = buff_start;
		buff_start += fb->bytesize;
	}
}

int handle_mmap_error(int err_number, char* msg, size_t len){

	switch (err_number) {
		case EACCES:
			// Input file not opened for read???
			strncpy(msg, "MMAP: Input file was not opened for reading.", len);
			return EX_SOFTWARE;
			break;

		case EINVAL:
			// most likely a programming error where offset is negative
			// or offset and/or size are not multiples of pagesize.
			strncpy(msg, "MMAP: offset or size may be < 0 or not multiple of pagesize.", len);
			return EX_SOFTWARE;
			break;

		case ENODEV:
			// file does not support mapping? File was a bit stream rather than
			// a file on disk?
			strncpy(msg, "MMAP: File does not support mapping.", len);
			return EX_OSERR;
			break;

		case ENOMEM:
			// no mem available
			strncpy(msg, "MMAP: Out of Memory.", len);
			return EX_OSERR;
			break;

		case ENXIO:
			// invalid adresses for file
			strncpy(msg, "MMAP: Invalid addresses for input file.", len);
			return EX_OSERR;
			break;

		case EOVERFLOW:
			// trying to read more than the size of the file
			strncpy(msg, "MMAP: Addresses above max offset set by input file.", len);
			return EX_SOFTWARE;
			break;

		//===== impossible cases: =====
		default:
			strncpy(msg, "MMAP: Unexpected errno.", len);
			return EX_SOFTWARE;
	}

}

typedef enum {
	DC_OK                 = 0,
	DC_CANTCREAT          = 1,
	DC_OSERR              = 2,
	DC_IOERR              = 3,
	DC_NON_HIDDEN_ENTRIES = 4,
	DC_PATH_TOO_LONG      = 5
} DirCheckError;

#if defined(_WIN32)
DirCheckError check_or_create_dest_dir(char* dest_dir){
	HANDLE dirhandle = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA dir_ffd;

	dirhandle = FindFirstFile(dest_dir, &dir_ffd);
	int dest_errval = GetLastError();

	if (dirhandle == INVALID_HANDLE_VALUE && dest_errval == ERROR_FILE_NOT_FOUND) {

		int success = CreateDirectoryA(dest_dir, NULL);
		if (success) {
			printf("Destination dir successfully created" ENDL);
			return DC_OK;
		}

		int errval = GetLastError();
		if (errval == ERROR_PATH_NOT_FOUND) {
			printf(
				"Error: One or more parent directories of the destination "
				"directory are missing. The destination directory couldn't be"
				"created." ENDL
			);
			return DC_CANTCREAT;
		}

		printf(
			"An unexpected error occured, "
			"the program will terminate now." ENDL
		);
		return DC_OSERR;
	}

	if (!(dir_ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		printf("Error: Destination path does not point to a directory." ENDL);
		return DC_CANTCREAT;
	}

	char dirglob[MAXIMUM_PATH()];
	if (strlen(dest_dir) + 3 > MAXIMUM_PATH()) {
		printf("Error: The destination path is too long" ENDL);
		return DC_PATH_TOO_LONG;
	}

	strcpy(dirglob, dest_dir);
	strcat(dirglob, "\\*");

	WIN32_FIND_DATA ffd;
	HANDLE h = INVALID_HANDLE_VALUE;

	h = FindFirstFileA((LPCSTR) dirglob, &ffd);
	if (h == INVALID_HANDLE_VALUE) {
		return DC_OSERR;
	}

	int hidden_entries = 0;
	int non_hidden_entries = 0;

	do {
		if (ffd.cFileName[0] == '.' || ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
			hidden_entries++;
			continue;
		}

		non_hidden_entries++;
		printf("Output directory contains a non hidden entry:" ENDL);
		printf("%s" ENDL, ffd.cFileName);
	} while (FindNextFile(h, &ffd) != 0);

	if (hidden_entries) {
		printf("%d hidden entries found, continuing" ENDL, hidden_entries);
	}

	if (non_hidden_entries) {
		printf("Error: %d non hidden entries found" ENDL, non_hidden_entries);
		return DC_NON_HIDDEN_ENTRIES;
	}


	return DC_OK;
}
#elif defined(__APPLE__) || defined(__LINUX__)
DirCheckError check_or_create_dest_dir(char* dest_dir) {
	DIR *dp;
	struct dirent *ep;

	errno = 0;
	dp = opendir(dest_dir);

	if (dp == NULL) {

		if (errno == ENOENT) {
			printf("output dir does not exist, creating it..." ENDL);

			if (mkdir(dest_dir, S_IRWXU)) {
				printf("failed creating dir" ENDL);
				return DC_CANTCREAT;
			}

			return DC_OK;

		} else {
			printf("an error occured while opening the output directory");
			return DC_OSERR;
		}
	}

	int hidden_entries = 0;
	int non_hidden_entries = 0;

	while ((ep = readdir(dp))) {

		if (ep->d_name[0] != '.') {

			non_hidden_entries++;

			printf("Output directory contains a non hidden entry:" ENDL);

			switch (ep->d_type){
				case DT_REG:
					printf("`%s`, a regular file" ENDL, ep->d_name);
					break;

				case DT_DIR:
					printf("`%s`, a regular directory" ENDL, ep->d_name);
					break;

				case DT_LNK:
					printf("`%s`, a symlink" ENDL, ep->d_name);
					break;

				case DT_FIFO:
				case DT_SOCK:
				case DT_CHR:
				case DT_BLK:
					printf("`%s`, a special file" ENDL, ep->d_name);
					break;

				case DT_UNKNOWN:
				default:
					printf("`%s`, an unknown entry type" ENDL, ep->d_name);
					break;
			}
		} else {
			// is entry just current dir or parent dir
			char cur_par = strcmp(ep->d_name, ".") || strcmp(ep->d_name, "..");
			if (!cur_par) hidden_entries++;
		}
	}

	if (closedir(dp)) {
		printf("an error occured while closing the output directory" ENDL);
		return DC_OSERR;
	}

	if (non_hidden_entries) return DC_NON_HIDDEN_ENTRIES;

	if (hidden_entries) {
		printf(
			"Warning: there are %d hidden files and/or directories in"
			" the output directory. They will be ignored." ENDL, hidden_entries
		);
	}

	return DC_OK;
}
#endif

void handle_dest_dir_check(DirCheckError err) {
	switch (err) {
		case DC_CANTCREAT:
			die("could not create output dir", EX_CANTCREAT);
			break;
		case DC_IOERR:
			die("could not open output dir for verification", EX_IOERR);
			break;
		case DC_OSERR:
			die("could not open or close output dir for verification", EX_OSERR);
			break;
		case DC_NON_HIDDEN_ENTRIES:
			die("the destination directory contains files but was expected to be empty", EX_TEMPFAIL);
			break;
		case DC_OK:
			break;
		default:
			die("unexpected codepath reached while checking output dir", EX_SOFTWARE);
			break;
		
	}
}

void output_open_print_err(int err) {
	switch (err) {
		case EACCES:
			printf("Writing autorization to file denied" ENDL);
			break;

		case EMFILE:
#if !_WIN32
		case EDQUOT:
#endif
		case ENOSPC:
			printf("Out of disk quota, too many inodes, or too many files opened" ENDL);
			break;

		case EEXIST:
			printf("file already exists!!!" ENDL);
			break;

		case EAGAIN:
		case EISDIR:
		case ENXIO:
		case EOPNOTSUPP:
		case EROFS:
		case ETXTBSY:
			printf("file is not writable!!!" ENDL);
			break;

		case EINTR:
			printf("Interrupted by a signal" ENDL);
			break;

		case ELOOP:
			printf("Too many symlinks" ENDL);
			break;

		case ENAMETOOLONG:
			printf("path element too long" ENDL);
			break;

		case ENOTDIR:
			printf("one of the elements in the path may not be a dir" ENDL);
			break;

		case EILSEQ:
		case EBADF:
		case EOVERFLOW:
		case EDEADLK:
		case ENOENT:
		case EFAULT:
		case EINVAL:
		case EIO:
		default:
			printf("Unexpected error n°%d: %s" ENDL, err, strerror(err));
			break;
	}
}

void output_fullfile_open_print_err(int err) {
	switch (err) {
		case EACCES:
			printf("Writing autorization to file denied" ENDL);
			break;

		case EMFILE:
		#if !_WIN32
		case EDQUOT:
		#endif
		case ENOSPC:
			printf("Out of disk quota, too many inodes, or too many files opened" ENDL);
			break;

		case EAGAIN:
		case EISDIR:
		case ENXIO:
		case EOPNOTSUPP:
		case EROFS:
		case ETXTBSY:
			printf("file is not writable!!!" ENDL);
			break;

		case EINTR:
			printf("Interrupted by a signal" ENDL);
			break;

		case ELOOP:
			printf("Too many symlinks" ENDL);
			break;

		case ENAMETOOLONG:
			printf("path element too long" ENDL);
			break;

		case ENOTDIR:
			printf("one of the elements in the path may not be a dir" ENDL);
			break;

		case EEXIST:
		case EILSEQ:
		case EBADF:
		case EOVERFLOW:
		case EDEADLK:
		case ENOENT:
		case EFAULT:
		case EINVAL:
		case EIO:
		default:
			printf("Unexpected error n°%d: %s" ENDL, err, strerror(err));
			break;
	}
}

void subsample(CompBuffer* cpb, ProcValBuffer* pvb) {
	// redefined with a shorter name within this scope
	int r_len = pvb->row_length;
	int r_cnt = pvb->row_count;
	
	for(int row_off = 0; row_off < r_cnt * r_len; row_off+=r_len) {
		for(int col_off = 0; col_off < r_len; col_off++) {

			float *from = cpb->start + 2 * (row_off + col_off);
			float *to = pvb->start + (row_off + col_off);

			//avg
			*to = (from[0] + from[1] + from[r_len] + from[r_len + 1]) / 4;
		}
	}
}

int read_chunk(
	const ReadBuffer *rd,
	CompBuffer *cp,
	const RowLayout *row_lo,
	MapOffsets *off,
	uint64_t file_size,
	char* read_complete_flag
){
	int read_rows = 0;

	// align correctly to the begining of the line,
	// within the first mapped page
	char *readptr = rd->start + off->page_to_readptr;
	*read_complete_flag = 0;

	// depends on
	// cpbuff
	for (;read_rows < cp->row_count; read_rows++) {
		off->fstart_to_readptr = off->fstart_to_page + (readptr - rd->start);

		// Can't check EOF flag since it's MMAP and not a file reading utility.
		// check for eof by comparing true offset to file size
		if (off->fstart_to_readptr > file_size) {
			*read_complete_flag = 1;
			break;
		}
		// flag for fields too big or too small...
		// int f2big = 0;
		// int f2sml = 0;

		float *cb_init_pos = cp->start + read_rows * cp->row_length;
		float *cb_row_limit = cb_init_pos + cp->row_length;

		if (off->fstart_to_readptr < file_size - row_lo->max_size) {

			// repeated cb_info.row_size (=row_lo.field_count) times
			// does not exit nor break to reduce code branching
			//
			for (float *cbidx=cb_init_pos; cbidx<cb_row_limit; cbidx++) {
				char *newptr = readptr;
				*cbidx = strtof(readptr, &newptr);

				//short delta = newptr - readptr;
				// f2big += delta > row_lo->max_field_size;
				// f2sml += delta < row_lo->min_field_size;

				readptr = newptr + 1;
			}

		} else {
			char *read_limit = rd->start + (file_size - off->fstart_to_page);
			for (float *cbidx=cb_init_pos; cbidx<cb_row_limit && readptr < read_limit; cbidx++) {
				char *newptr = readptr;
				*cbidx = strtof(readptr, &newptr);

				//short delta = newptr - readptr;
				// f2big += delta > row_lo->max_field_size;
				// f2sml += delta < row_lo->min_field_size;

				readptr = newptr + 1;
			}
		}

		if (row_lo->eol_size == 2) readptr++;
		// if (f2big || f2sml){
		// 	printf("unexpected size of field:");
		// 	if (f2big) printf("    too big x %d", f2big);
		// 	if (f2sml) printf("    too small x %d", f2sml);
		// 	printf(ENDL);
		//
		// }
	}
	
	// grow input_offset
	if (!*read_complete_flag) {
		off->fstart_to_readptr = off->fstart_to_page + (readptr - rd->start);
		off->page_to_readptr = off->fstart_to_readptr % rd->page_bytesize;
		off->fstart_to_page = off->fstart_to_readptr - off->page_to_readptr;
	}
	return read_rows;
}

void write_buffers_to_files(WriteBuffer *wr, Config* cf, int tile_row){
	for (int i=0; i<wr->file_buffer_count; i++) {
		// generate file path
		// due diligence done at beginning of main,
		// if there are any error while creating the file
		// skip to next file
		char path[MAXIMUM_PATH()];
		int char_count =
			snprintf(path, MAXIMUM_PATH(), "%s/row%.3d_col%.3d.csv", cf->dest, tile_row, i);
		if (char_count >= MAXIMUM_PATH()) {
			printf(ENDL);
			die("pathname too big!", EX_SOFTWARE);
		}

		errno = 0;
		FILE *fp = fopen(path, "wb");
		int err = errno;

		if (fp == NULL) {
			printf("an error occured while opening an output file" ENDL);
			printf("path: %s" ENDL, path);

			output_open_print_err(err);

			printf("skipping..." ENDL);
		}
		else {
			// fill buffer
			FileBuffer *fb = wr->file_buffers + i;

			// TODO: handle write errors
			errno = 0;
			unsigned int written_bytes = fwrite(fb->buffer, 1, fb->bytesize, fp);
			int errval = errno;

			if (written_bytes < 0) {
				printf(
					"ERROR n°%d: %s while writing to file %s" ENDL,
					errval, strerror(errval), path
				);
			}

			else if (written_bytes != fb->bytesize) {
				printf(
					"error: discrepancy between buffer size and number of bytes"
					"written... : expected %llu, wrote %d" ENDL,
					(long long unsigned) fb->bytesize,
					written_bytes
				);
			}

			fclose(fp);
		}
	}
}

int write_FullFileBuffer_to_file(FullFileBuffer *ff, Config* cf){
	char path[MAXIMUM_PATH()];
	int char_count =
		snprintf(path, MAXIMUM_PATH(), "%s/resized_full.csv", cf->dest);
	if (char_count >= MAXIMUM_PATH()) {
		printf(ENDL);
		die("pathname too big!", EX_SOFTWARE);
	}

	// file at `path` will be created, written to, then closed,
	// then opened, then appended to, then closed,
	// then opened, then appended to, then closed,
	// ...

	errno = 0;
	FILE *fp = fopen(path, "ab+");
	int err = errno;

	if (fp == NULL) {
		printf("an error occured while opening an output file" ENDL);
		printf("path: %s" ENDL, path);

		output_fullfile_open_print_err(err);
		return 1;
	}

	errno = 0;
	unsigned int written_bytes = fwrite(ff->buffer, 1, ff->bytesize, fp);
	int errval = errno;

	if (written_bytes < 0) {
		printf(
			"ERROR n°%d: %s while writing to file %s" ENDL,
			errval, strerror(errval), path
		);
		fclose(fp);
		return 1;
	}

	if (written_bytes != ff->bytesize) {
		printf(
			"error: discrepancy between buffer size and number of bytes"
			"written... : expected %llu, wrote %d" ENDL,
			(long long unsigned) ff->bytesize,
			written_bytes
		);
	}

	fclose(fp);
	return 0;
}

int fill_filebuffers(ProcValBuffer *pv, WriteBuffer *wr){
	// offset between the beginning of pv buffer and the beginning
	// of the current row
	float *row_start = pv->start;
	int field_sz =  wr->field_size;
	int stride = wr->field_size + wr->sep_size;

	int write_overflow = 0;

	for (int row_idx=0; row_idx < pv->row_count; row_idx++) {

		// if (!((row_idx + 1) % 100)) printf("rows written to buffer = %d" ENDL,row_idx);

		// offset between the beginning of the row and the beginning of
		// the range relevant to the current file.
		float *range_start = row_start;

		// if (row_idx + 1 == pv->row_count) printf("writing last row" ENDL);

		for (int f_idx=0; f_idx < wr->file_buffer_count; f_idx++){

			// if (row_idx + 1 == pv->row_count) printf("file buffer n°%d is being written to" ENDL, f_idx);

			FileBuffer file = wr->file_buffers[f_idx];

			char *fb_ptr = file.buffer + row_idx * file.row_size;

			float *range_end = range_start + file.row_length;

			// if the value is too big, we risk losing precision at best
			// and doing a segfault at worst.
			// I will not check for theses cases for performance, but I will try to educate
			// the user about it, to prevent corruption.
			for (float *val_ptr = range_start; val_ptr < range_end; val_ptr++){
				// PERF: Investigate if loop unrolling with multiple %f is worth it

				int count = snprintf(fb_ptr, wr->field_size + 1, "%0*.3f", field_sz, *val_ptr); // + 1 for the \0
				
				write_overflow += count != field_sz;
				
				//write the comma afterwards
				fb_ptr[wr->field_size] = ',';
	
				fb_ptr += stride;
			}
			// remove extra sep
			// write newline
			if (wr->eol_size == 1) {
				fb_ptr[-1] = '\n';
			} else {
				fb_ptr[-1] = '\r';
				fb_ptr[ 0] = '\n';
			}
			range_start = range_end;
		}
		row_start += pv->row_length;
	}
	return write_overflow;
}

void fill_fullfile_buffer(FullFileBuffer *ff, WriteBuffer *wr){
	char* writeptr = ff->buffer;
	for (int row_idx = 0; row_idx < ff->row_count; row_idx++){
		
		FileBuffer *file_stop = wr->file_buffers + wr->file_buffer_count;
		for (FileBuffer *file = wr->file_buffers; file < file_stop; file++){

			char *src_row = file->buffer + row_idx * file->row_size;
			size_t segment_length = file->row_size - ff->eol_size;

			memcpy((void *) writeptr, (void *) src_row, segment_length);
			writeptr += segment_length;
			*writeptr++ = ',';
		}
		writeptr--; // otherwise we get a free extra comma... and a buffer overrun
		if (ff->eol_size > 1) *writeptr++ = '\r';
		*writeptr++ = '\n';
		ptrdiff_t delta = writeptr - ff->buffer;
		ptrdiff_t expected = ff->row_bytesize * (row_idx + 1);
		if (delta != expected) printf("wrote too much on this row"ENDL);
	}
}

#if defined(__APPLE__) || defined(__LINUX__)
/*! initializes the row_layout struct passed in argument
 *
 * @param valid pointer to the struct to initialize.
 * @param valid pointer to a valid Config struct.
 * @param input_fd a valid file descriptor that can be read.
 * @param valid pointer to an ErrMsg struct with its msg
 *        field initialized to 0.
 *
 * @return 0 if the 1st row of input_fd was parsed successfully.
 *         Otherwise returns 1 and sets the ErrMsg struct pointed
 *         by err accordingly.
 */
int get_row_layout(
	RowLayout *  row_lo,
	const Config * conf,
	int input_fd,
	ErrMsg * err
) {
	RowInfo info = {0};
	int errval = identify_L1(&info, input_fd);

	if (errval) {
		strncpy(err->msg, "Failed parsing 1st row of the input file", ERR_MSG_SIZE);
		err->val = errval;
		return 1;
	}

	if (init_RowLayout(row_lo, &info, conf)) {
		strncpy(err->msg, "Inconclusive eol configuration and detection", ERR_MSG_SIZE);
		err->val = EX_DATAERR;
		return 1;
	}

	return 0;
}
#endif

int get_row_layout_from_fp(
	RowLayout *row_lo,
	const Config *conf,
	FILE *fp,
	ErrMsg *err
) {
	RowInfo info = {0};
	int errval = identify_L1_fp(&info, fp);

	if (errval) {
		strncpy(err->msg, "Failed parsing 1st row of the input file", ERR_MSG_SIZE);
		err->val = errval;
		return 1;
	}

	if (init_RowLayout(row_lo, &info, conf)) {
		strncpy(err->msg, "Inconclusive eol configuration and detection", ERR_MSG_SIZE);
		err->val = EX_DATAERR;
		return 1;
	}

	return 0;
}

#if defined(_WIN32)
/*! Gets the file handle pointed at by the path
 * @param path must be a valid c string
 * @param err must be a valid pointer to a ErrMsg struct
 *
 * @return 0 if the handle
 */
HANDLE get_normal_file_handle(char* path, ErrMsg* err) {
	HANDLE search_handle = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA ffd;

	search_handle = FindFirstFile(path, &ffd);
	if (search_handle == INVALID_HANDLE_VALUE) {
		strncpy(
			err->msg,
			"Couldn't acquire file handle for input file... Exiting",
			ERR_MSG_SIZE
		);
		err->val = EX_OSERR;
		return INVALID_HANDLE_VALUE;
	}

	DWORD attrs = ffd.dwFileAttributes;
	ULONG blacklist = (
		FILE_ATTRIBUTE_DIRECTORY
		| FILE_ATTRIBUTE_DEVICE
		| FILE_ATTRIBUTE_VIRTUAL
	);
	if (attrs & blacklist) {
		printf("input file has the following Attributes: %lx" ENDL, attrs);
		print_file_attributes(attrs);
		strncpy(
			err->msg,
			"Input path may not point to a file,"
			" or may not be stored locally.",
			ERR_MSG_SIZE
		);
		err->val = EX_DATAERR;
		return INVALID_HANDLE_VALUE;
	}

	HANDLE handle = CreateFile(
		path,
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN,
		NULL
	);

	if (handle == INVALID_HANDLE_VALUE) {
		printf("failed to create file handle, err code: %ld"ENDL, GetLastError());
		strncpy(
			err->msg,
			"Input path may not point to a file,"
			" or may not be stored locally.",
			ERR_MSG_SIZE
		);
	}

	return handle;
}

/*! Create a non-sharable file mapping object
 *  of a whole file for read-only purposes.
 *
 *  @param file_handle is a valid handle of a normal file.
 *  @param file_size is the size of the file we wish to map.
 *
 *  @return a handle that can be valid or not, depending on success.
 */
#endif


int main(int argc, char* argv[]){
	/*
	*	Initialization phase:
	*
	*	init a bunch of variables
	*	compute a bunch of useful values
	*	alloc some memory for some specific buffers
	*
	*/

	if (argc != 2) die("Wrong number of arguments", EX_USAGE);

	// get config
	Config conf = {0};

	printf("reading config file" ENDL);
	if (get_config(argv[1], &conf)) die("Invalid config file", EX_DATAERR);

	int dest_dir_err = check_or_create_dest_dir(conf.dest);
	if (dest_dir_err) handle_dest_dir_check(dest_dir_err);

	// open source file
	printf("input file path = `%s`" ENDL, conf.source);
	#ifdef _WIN32
	input_fp = fopen(conf.source, "rb");

	if (atexit(close_input_fp)) {
		die("could not set file auto-closing at exit", EX_SOFTWARE);
	}
	if (input_fp == NULL) specify_os_error_and_exit();
	#else
	input_fd = open(conf.source, O_RDONLY);
	if (atexit(close_input_fd)) {
		die("could not set file auto-closing at exit", EX_SOFTWARE);
	}
	// printf("input file descriptor = %d" ENDL, input_fd);
	if (input_fd < 0) specify_os_error_and_exit();
	#endif

	ErrMsg rl_err = {0};
	RowLayout row_lo = {0};
	#ifdef _WIN32
	if (get_row_layout_from_fp(&row_lo, &conf, input_fp, &rl_err)) {
		die(rl_err.msg, rl_err.val);
	}
	#else
	if (get_row_layout(&row_lo, &conf, input_fd, &rl_err)) {
		die(rl_err.msg, rl_err.val);
	}
	#endif

	uint64_t file_size = 0;

	#if defined(__APPLE__) || defined(__LINUX__)
	file_size = get_file_size_fd(input_fd);
	if (file_size == 0) die("could not read source file stats", EX_OSERR);

	#elif defined(_WIN32)
	// on windows platforms, we will use a HANDLE pointer
	// for the rest of the program.
	if (fclose(input_fp)) die("could not close input file correctly", EX_OSERR);

	ErrMsg filehandle_err = {0};
	HANDLE input_handle = get_normal_file_handle(conf.source, &filehandle_err);
	if (input_handle == INVALID_HANDLE_VALUE) {
		die(filehandle_err.msg, filehandle_err.val);
	}

	file_size = file_size_from_handle(input_handle);
	if (file_size == 0) die("could not read source file stats", EX_SOFTWARE);

	HANDLE map_handle = CreateFileMapping(
		input_handle, NULL, PAGE_READONLY, 0, 0, NULL
	);
	if (map_handle == NULL)
		die("could not create file mapping object", EX_OSERR);
	#endif


	ReadBuffer rdbuff = {0};
	init_ReadBufferStruct(&rdbuff, &row_lo, &conf);

	CompBuffer cpbuff = {0};
	if (init_CompBuffer(&cpbuff, &row_lo, &conf)) die("Out of memory", EX_SOFTWARE);
	// no malloc -> only done when the number of read rows has been counted
	// (can change from chunk to chunk)

	ProcValBuffer pvbuff = {0};
	// no malloc -> only done when compbuff has updated its size
	init_ProcValBufferStruct(&pvbuff, &row_lo, &conf);
	
	// get source file size
	printf("getting input file statistics" ENDL);



	// printf("Input file size: " ENDL);
	// print_size_info(file_size);

	// counts the rank of the last processed row of tiles

	int tile_row = 0;

	MapOffsets map_offsets = {
		.fstart_to_page = 0,
		.page_to_readptr = 0,
		.fstart_to_readptr = 0
	};

	/*
	 *=========================== Processing phase ============================
	 */

	printf("Setup finished, starting processing" ENDL);

	char INPUT_READING_COMPLETE = 0;
	int FULLFILE_FAILED = 0;

	// We don't know the number of rows in advance so no for loop
	while(!INPUT_READING_COMPLETE) {
		printf("processing chunk [%d]" ENDL, tile_row);

		#if defined(__APPLE__) || defined(__LINUX__)
		errno = 0;
		rdbuff.start = mmap( // PERF: could be optimized by using the MAP_FIXED flag?
			NULL,
			rdbuff.bytesize,
			PROT_READ,
			MAP_PRIVATE|MAP_FILE,
			input_fd,
			map_offsets.fstart_to_page
		);

		if (rdbuff.start == MAP_FAILED) {
			char msg[ERR_MSG_SIZE] = {0};
			int err = handle_mmap_error(errno, msg, ERR_MSG_SIZE);
			die(msg, err);
		}
		#elif defined(_WIN32)
		BIG_WORD bwSize = {.full = map_offsets.fstart_to_page};
		int64_t remaining_space = file_size - map_offsets.fstart_to_page;
		int64_t map_size = rdbuff.bytesize < remaining_space ? rdbuff.bytesize : 0; // 0 means rest of file
		rdbuff.start = MapViewOfFile(
			map_handle,
			FILE_MAP_READ,
			bwSize.parts[1], bwSize.parts[0], //little-endian only
			map_size
		);

		if (rdbuff.start == NULL){
			die("Couldn't map a view of input file", EX_OSERR);
		}
		#endif

		printf("file successfully mapped to memory [%d]" ENDL, tile_row);


		int read_rows = read_chunk(
			&rdbuff, &cpbuff, &row_lo,
			&map_offsets, file_size,
			&INPUT_READING_COMPLETE
		);

		printf("data successfully converted to float [%d]" ENDL, tile_row);

		#if defined(__APPLE__) || defined(__LINUX__)
		munmap(rdbuff.start, rdbuff.bytesize); // size == byte_size since sizeof(char) == 1
		#elif defined(_WIN32)
		UnmapViewOfFile(rdbuff.start);
		#endif

		// comp
		// Only compute as much as was parsed
		if (INPUT_READING_COMPLETE) {
			// calc write buff row count again
			printf("last chunk reached [%d]" ENDL, tile_row);
			pvbuff.row_count = read_rows / 2;
			pvbuff.bytesize = (int64_t) pvbuff.row_count * pvbuff.row_length;
		}

		pvbuff.start = (float *) malloc(pvbuff.bytesize);
		if (pvbuff.start == NULL) die("Out of Memory (malloc pvbuff).", EX_OSERR);

		subsample(&cpbuff, &pvbuff);
		
		printf("subsampling finished [%d]" ENDL, tile_row);

		WriteBuffer wrbuff = {0};
		if (init_WriteBufferStruct(&wrbuff, &pvbuff, &conf))
			die("Out of Memory (malloc wrbuff->file_buffers)", EX_OSERR);

		wrbuff.buffer = (char *) malloc(wrbuff.bytesize);
		if(wrbuff.buffer == NULL)
			die("Out of Memory (malloc wrbuff->buffer)", EX_OSERR);

		asign_filebuffers(&wrbuff);

		FullFileBuffer ffbuff = {0};
		init_FullFileBuffer(
			&ffbuff,
			pvbuff.row_length,
			pvbuff.row_count,
			conf.output_field_size,
			row_lo.sep_size,
			row_lo.eol_size
		);
		ffbuff.buffer = malloc(ffbuff.bytesize);
		if(ffbuff.buffer == NULL)
			die("Out of Memory (malloc ffbuff->buffer)", EX_OSERR);

		printf("filling file buffers [%d]" ENDL, tile_row);
		fill_filebuffers(&pvbuff, &wrbuff);
		fill_fullfile_buffer(&ffbuff, &wrbuff);

		printf("writing to files [%d]" ENDL, tile_row);
		write_buffers_to_files(&wrbuff, &conf, tile_row);
		if (!FULLFILE_FAILED) {
			FULLFILE_FAILED = write_FullFileBuffer_to_file(&ffbuff, &conf);
		}


		free(pvbuff.start);
		free(wrbuff.file_buffers);
		free(wrbuff.buffer);
		free(ffbuff.buffer);

		printf("chunk processed [%d]" ENDL, tile_row);

		tile_row++;
	}

	/*
	 *============================= Debrief phase =============================
	 */

	// print a report (number of rows parsed, errors? exceptions? exec time,
	// average time per parsed value etc)
	// exit
	exit(EX_OK);
}
