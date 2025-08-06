#include <errno.h>
#include <fcntl.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <dirent.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "../include/file_identificator.h"
#include "../include/arg_parse.h"
#include "../include/buffer_util.h"
#include "../include/utils.h"

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

#define ERR_MSG_SIZE 1000 // Arbitrary value
#define SMALL_ERR_MSG_SIZE 100 // Arbitrary value
#define PARSING_ERR_LIMIT 5

# define USAGE "Usage: benchmark [config path]\n"

# define die(e_msg, ex_no) _die(e_msg, ex_no, USAGE)

// ================================= GLOBALS ==================================

static int input_fd = -1;
static void* comp_buff_ptr = NULL;

// ============================= ATEXIT FUNCTIONS =============================

void close_input(void){
	close(input_fd);
}

void free_comp_buff(void){
	free(comp_buff_ptr);
}

// ================================= THE REST =================================


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
	snprintf(msg, ERR_MSG_SIZE, "Unexpected syscall errno %d : ", errno);
	char* msg_end = index(msg, '\0');
	size_t rem_space = msg + ERR_MSG_SIZE - msg_end;
	strerror_r(errno, msg_end, rem_space);
	die(msg, EX_SOFTWARE);
}

void print_size_info(off_t bytes){
	off_t total_count = bytes;
	const char log2_1024 = 10;

	off_t B = total_count % 1024;
	total_count <<= log2_1024;
	off_t KiB = (total_count) % 1024;
	total_count <<= log2_1024;
	off_t MiB = (total_count) % 1024;
	total_count <<= log2_1024;
	off_t GiB = (total_count) % 1024;
	total_count <<= log2_1024;
	off_t TiB = (total_count) % 1024;

	printf("Object is of size : %lli bytes\n", bytes);
	printf("or %lli TiB, %lli GiB, %lli MiB, %lli KiB & %lli bytes.\n",
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
			   "fallback option was provided\n");
		return EOL_AUTO;
	}
	if (as_detected == EOL_AUTO) {
		printf("WARNING: no end of line was detected, falling back to configuration\n");
	} else if (from_config != as_detected && from_config != EOL_AUTO) {
		printf("WARNING: The end-of-line marker specified in the configuration does"
			   " not match\nwith the one detected. Falling back to configuration\n");
	}
	return (from_config == EOL_AUTO) ? as_detected : from_config;
}

int parse_readbuffer_line(char* start, char** end, ParserConfig* conf, float* outptr) {
	char* current = start;
	char* prev = start;
	char* fend;

	char endl = conf->line.eol == EOL_UNIX ? '\n':'\r';
	char errcount = 0;
	off_t offset;

	for (
		size_t count = 0 ; (count < conf->line.field_count) ; count++) {
		errno = 0;
		fend = current;
		outptr[count] = strtof(current, &fend);

		if (fend == current) {
			fend = index(current, ','); // TODO: replace by a `sep` variable
			if (fend == NULL) errcount += PARSING_ERR_LIMIT;
		}

		offset = fend - current;
		if (
			((size_t) offset < conf->field.min)
			|| ((size_t) offset > conf->field.max)
			|| errno
		) { //write down error in flag
			errcount += 1;
		}

		prev = fend;
		current = fend + 1;

		if ((*prev == endl) || (errcount >= PARSING_ERR_LIMIT)) break;
	}

	if (*fend == '\n') fend++;
	*end = fend;

	return errcount;
}

int parse_chunk(char** start, Config* conf, ParserConfig* pconf, float** outptr) {
	//	should provide:
	//		ptr to end of current readchunk
	//		ptr to end of current compute chunk
	//		error status
	char* current_rd = *start;
	char* end_rd;
	float* current_out = *outptr;
	int UNRECOVERABLE = 0;
	int errcount = 0;

	for (int row = 0; row < (conf->tile_height * 2); row ++) {
		errcount += parse_readbuffer_line(current_rd, &end_rd, pconf, current_out);
		if (errcount >= PARSING_ERR_LIMIT) {
			printf("too many errors\n");
			UNRECOVERABLE = 1;
			break;
		}
		current_rd = end_rd;
		current_out += (pconf->line.field_count * 2);
	}
	*start = current_rd;
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
	rl->max_size = (cf->max_field_size + rl->sep_size) * ri->count - rl->sep_size + rl->eol_size;
	return 0;
}

int init_CompBuffer(CompBuffer *cb, const RowLayout *row_lo, const Config *cf) {
	init_CompBufferStruct(cb, row_lo, cf);
	cb->start = (float *) malloc(cb->bytesize);
	if (cb->start == NULL) {
		printf("couldn't allocate memory for computation buffer\n");
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
	int sep = 1;
	int stride = conf->output_field_size + sep;
	int eol = conf->eol_flag == EOL_UNIX ? 1 : 2;

	// we don't malloc the whole buffer
	wb->buffer = NULL;

	// but we malloc the array of FileBuffers (not actual buffers)
	div_t qr = div(pvb->row_length, conf->tile_width);
	int file_count = qr.quot + (qr.rem ? 1 : 0);
	wb->file_buffers = (FileBuffer *) malloc(file_count * sizeof(FileBuffer));

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
		fb->bytesize = fb->row_size * pvb->row_count;
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
			strlcpy(msg, "MMAP: Input file was not opened for reading.", len);	
			return EX_SOFTWARE;
			break;

		case EINVAL:
			// most likely a programming error where offset is negative
			// or offset and/or size are not multiples of pagesize.
			strlcpy(msg, "MMAP: offset or size may be < 0 or not multiple of pagesize.", len);
			return EX_SOFTWARE;
			break;

		case ENODEV:
			// file does not support mapping? File was a bit stream rather than
			// a file on disk?
			strlcpy(msg, "MMAP: File does not support mapping.", len);
			return EX_OSERR;
			break;

		case ENOMEM:
			// no mem available
			strlcpy(msg, "MMAP: Out of Memory.", len);
			return EX_OSERR;
			break;

		case ENXIO:
			// invalid adresses for file
			strlcpy(msg, "MMAP: Invalid addresses for input file.", len);
			return EX_OSERR;
			break;

		case EOVERFLOW:
			// trying to read more than the size of the file
			strlcpy(msg, "MMAP: Addresses above max offset set by input file.", len);
			return EX_SOFTWARE;
			break;

		//===== impossible cases: =====
		default:
			strlcpy(msg, "MMAP: Unexpected errno.", len);
			return EX_SOFTWARE;
	}

}

int check_dest_dir(char* dest_dir) {
	DIR *dp;
	struct dirent *ep;

	errno = 0;
	dp = opendir(dest_dir);

	if (dp == NULL) {

		if (errno == ENOENT) {
			printf("output dir does not exist, creating it...\n");

			if (mkdir(dest_dir, S_IRWXU)) {
				printf("failed creating dir\n");
				return EX_CANTCREAT;
			}

			dp = opendir(dest_dir);

			if (dp == NULL) {
				printf("failed opening created dir\n");
				return EX_OSERR;
			}

		} else {
			printf("an error occured while opening the output directory");
			return EX_IOERR;
		}
	}

	int hidden_entries = 0;
	int non_hidden_entries = 0;

	while ((ep = readdir(dp))) {

		if (ep->d_name[0] != '.') {

			non_hidden_entries++;

			printf("Output directory contains a non hidden entry:\n");

			switch (ep->d_type){
				case DT_REG:
					printf("`%s`, a regular file\n", ep->d_name);
					break;

				case DT_DIR:
					printf("`%s`, a regular directory\n", ep->d_name);
					break;

				case DT_LNK:
					printf("`%s`, a symlink\n", ep->d_name);
					break;

				case DT_FIFO:
				case DT_SOCK:
				case DT_CHR:
				case DT_BLK:
					printf("`%s`, a special file\n", ep->d_name);
					break;

				case DT_UNKNOWN:
				default:
					printf("`%s`, an unknown entry type\n", ep->d_name);
					break;
			}
		} else {
			// is entry just current dir or parent dir
			char cur_par = strcmp(ep->d_name, ".") || strcmp(ep->d_name, "..");
			if (!cur_par) hidden_entries++;
		}
	}

	if (closedir(dp)) {
		printf("an error occured while closing the output directory\n");
		return EX_OSERR;
	}

	if (non_hidden_entries) return EX_TEMPFAIL;

	if (hidden_entries) {
		printf(
			"Warning: there are %d hidden files and/or directories in"
			" the output directory. They will be ignored.\n", hidden_entries
		);
	}

	return EX_OK;
}

void handle_dest_dir_check(int err) {
	switch (err) {
		case EX_CANTCREAT:
			die("could not create output dir", err);
			break;
		case EX_IOERR:
			die("could not open output dir for verification", err);
			break;
		case EX_OSERR:
			die("could not open or close output dir for verification", err);
			break;
		case EX_TEMPFAIL:
			die("the destination directory contains files but was expected to be empty", err);
			break;
		case EX_OK:
			break;
		default:
			die("unexpected codepath reached while checking output dir", EX_SOFTWARE);
			break;
		
	}
}

void output_open_print_err(int err) {
	char errbuf[SMALL_ERR_MSG_SIZE];

	switch (err) {
		case EACCES:
			printf("Writing autorization to file denied\n");
			break;

		case EMFILE:
		case EDQUOT:
		case ENOSPC:
			printf("Out of disk quota, too many inodes, or too many files opened\n");
			break;

		case EEXIST:
			printf("file already exists!!!\n");
			break;

		case EAGAIN:
		case EISDIR:
		case ENXIO:
		case EOPNOTSUPP:
		case EROFS:
		case ETXTBSY:
			printf("file is not writable!!!\n");
			break;

		case EINTR:
			printf("Interrupted by a signal\n");
			break;

		case ELOOP:
			printf("Too many symlinks\n");
			break;

		case ENAMETOOLONG:
			printf("path element too long\n");
			break;

		case ENOTDIR:
			printf("one of the elements in the path may not be a dir\n");
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
			strerror_r(err, errbuf, sizeof(errbuf));
			printf("Unexpected error: %s\n", errbuf);
			break;
	}
}

void output_fullfile_open_print_err(int err) {
	char errbuf[SMALL_ERR_MSG_SIZE];

	switch (err) {
		case EACCES:
			printf("Writing autorization to file denied\n");
			break;

		case EMFILE:
		case EDQUOT:
		case ENOSPC:
			printf("Out of disk quota, too many inodes, or too many files opened\n");
			break;

		case EAGAIN:
		case EISDIR:
		case ENXIO:
		case EOPNOTSUPP:
		case EROFS:
		case ETXTBSY:
			printf("file is not writable!!!\n");
			break;

		case EINTR:
			printf("Interrupted by a signal\n");
			break;

		case ELOOP:
			printf("Too many symlinks\n");
			break;

		case ENAMETOOLONG:
			printf("path element too long\n");
			break;

		case ENOTDIR:
			printf("one of the elements in the path may not be a dir\n");
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
			strerror_r(err, errbuf, sizeof(errbuf));
			printf("Unexpected error: %s\n", errbuf);
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
	ReadBuffer *rd, CompBuffer *cp, RowLayout *row_lo,
	off_t *relative_offset, off_t *true_offset,
	off_t *file_offset, off_t file_size,
	char* WORK_FINISHED
){
	unsigned int read_rows = 0;

	// align correctly to the begining of the line,
	// within the first mapped page
	char *readptr = rd->start + *relative_offset;
	*WORK_FINISHED = 0;

	// depends on
	// cpbuff
	for (;read_rows < cp->row_count; read_rows++) {
		*true_offset = *file_offset + (readptr - rd->start);

		// Can't check EOF flag since it's MMAP and not a file reading utility.
		// check for eof by comparing true offset to file size
		if (*true_offset > file_size) {
			*WORK_FINISHED = 1;
			break;
		}


		
		// flag for fields too big or too small...
		// int f2big = 0;
		// int f2sml = 0;

		float *cb_init_pos = cp->start + read_rows * cp->row_length;
		float *cb_row_limit = cb_init_pos + cp->row_length;

		if (*true_offset < (off_t) (file_size - row_lo->max_size)) {

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
			char *read_limit = rd->start + (file_size - *file_offset);
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
		// 	printf("\n");
		//
		// }
	}
	
	// grow input_offset
	if (!*WORK_FINISHED) {
		*true_offset = *file_offset + (readptr - rd->start);
		*relative_offset = *true_offset % rd->page_bytesize;
		*file_offset = *true_offset - *relative_offset;
	}
	return read_rows;
}

void write_buffers_to_files(WriteBuffer *wr, Config* cf, int tile_row){
	for (unsigned int i=0; i<wr->file_buffer_count; i++) {
		// generate file path
		// due diligence done at beginning of main,
		// if there are any error while creating the file
		// skip to next file
		char path[PATH_MAX];
		int char_count =
			snprintf(path, PATH_MAX, "%s/row%.3d_col%.3d.csv", cf->dest, tile_row, i);
		if (char_count >= PATH_MAX) {
			printf("\n");
			die("pathname too big!", EX_SOFTWARE);
		}

		errno = 0;
		int openflags = O_WRONLY | O_CREAT | O_EXCL;
		int modflags = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		int ofd = open(path, openflags, modflags);
		int err = errno;

		if (ofd < 0) {
			printf("an error occured while opening an output file\n");
			printf("path: %s\n", path);

			output_open_print_err(err);

			printf("skipping...\n");
		}
		else {
			// fill buffer
			FileBuffer *fb = wr->file_buffers + i;

			// TODO: handle write errors
			errno = 0;
			unsigned int written_bytes = write(ofd, fb->buffer, fb->bytesize);
			int errval = errno;

			if (written_bytes < 0) {
				printf(
					"ERROR n°%d: %s while writing to file %s\n",
					errval, strerror(errval), path
				);
			}

			else if (written_bytes != fb->bytesize) {
				printf(
					"error: discrepancy between buffer size and number of bytes"
					"written... : expected %lu, wrote %d\n",
					fb->bytesize,
					written_bytes
				);
			}

			close(ofd);
		}
	}
}

int write_FullFileBuffer_to_file(FullFileBuffer *ff, Config* cf){
	char path[PATH_MAX];
	int char_count =
		snprintf(path, PATH_MAX, "%s/resized_full.csv", cf->dest);
	if (char_count >= PATH_MAX) {
		printf("\n");
		die("pathname too big!", EX_SOFTWARE);
	}

	// file at `path` will be created, written to, then closed,
	// then opened, then appended to, then closed,
	// then opened, then appended to, then closed,
	// ...

	errno = 0;
	int openflags = O_WRONLY | O_CREAT | O_APPEND;
	int modflags = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int ofd = open(path, openflags, modflags);
	int err = errno;

	if (ofd < 0) {
		printf("an error occured while opening an output file\n");
		printf("path: %s\n", path);

		output_fullfile_open_print_err(err);
		return 1;
	}

	errno = 0;
	unsigned int written_bytes = write(ofd, ff->buffer, ff->bytesize);
	int errval = errno;

	if (written_bytes < 0) {
		printf(
			"ERROR n°%d: %s while writing to file %s\n",
			errval, strerror(errval), path
		);
		close(ofd);
		return 1;
	}

	if (written_bytes != ff->bytesize) {
		printf(
			"error: discrepancy between buffer size and number of bytes"
			"written... : expected %lu, wrote %d\n",
			ff->bytesize,
			written_bytes
		);
	}

	close(ofd);
	return 0;
}

void fill_filebuffers(ProcValBuffer *pv, WriteBuffer *wr){
	// offset between the beginning of pv buffer and the beginning
	// of the current row
	float *row_start = pv->start;
	int field_sz =  wr->field_size;
	int stride = wr->field_size + wr->sep_size;

	for (unsigned int row_idx=0; row_idx < pv->row_count; row_idx++) {

		// if (!((row_idx + 1) % 100)) printf("rows written to buffer = %d\n",row_idx);

		// offset between the beginning of the row and the beginning of
		// the range relevant to the current file.
		float *range_start = row_start;

		// if (row_idx + 1 == pv->row_count) printf("writing last row\n");

		for (unsigned int f_idx=0; f_idx < wr->file_buffer_count; f_idx++){

			// if (row_idx + 1 == pv->row_count) printf("file buffer n°%d is being written to\n", f_idx);

			FileBuffer file = wr->file_buffers[f_idx];

			char *fb_ptr = file.buffer + row_idx * file.row_size;

			float *range_end = range_start + file.row_length;

			// if the value is too big, we risk losing precision at best
			// and doing a segfault at worst.
			// I will not check for theses cases for performance, but I will try to educate
			// the user about it, to prevent corruption.
			for (float *val_ptr = range_start; val_ptr < range_end; val_ptr++){
				// PERF: Investigate if loop unrolling with larger formatted
				// strings is worth it like :
				// print first field
				// ...
				// snprintf(fb_ptr, 4 * stride + 1,
				//          ",%0*.3f,%0*.3f,%0*.3f,%0*.3f",
				//          fsz, val[0], fsz, val[1], fsz, val[2], fsz, val[3]);
				// ...
				// and a simple loop for the remaining fields
				// ...
				// would be implemented on an OS by OS basis

				snprintf(fb_ptr, stride + 1, "%0*.3f,", field_sz, *val_ptr); // stride+1 for the \0
	
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
}

void fill_fullfile_buffer(FullFileBuffer *ff, WriteBuffer *wr){
	char* writeptr = ff->buffer;
	for (unsigned int row_idx = 0; row_idx < ff->row_count; row_idx++){
		
		FileBuffer *file_stop = wr->file_buffers + wr->file_buffer_count;
		for (FileBuffer *file = wr->file_buffers; file < file_stop; file++){

			char *src_row = file->buffer + row_idx * file->row_size;
			size_t segment_length = file->row_size - ff->eol_size;

			memcpy((void *) writeptr, (void *) src_row, segment_length);
			writeptr += segment_length;
			*writeptr++ = ',';
		}
		if (ff->eol_size > 1) *writeptr++ = '\r';
		*writeptr++ = '\n';
	}
}

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

	printf("reading config file\n");
	if (get_config(argv[1], &conf)) die("Invalid config file", EX_DATAERR);

	{ // isolating err to not clutter scope
		int err = check_dest_dir(conf.dest);
		handle_dest_dir_check(err);
	}

	// open source file
	printf("input file path = `%s`\n", conf.source);
	input_fd = open(conf.source, O_RDONLY);
	if (atexit(close_input)) {
		die("could not set file auto-closing at exit", EX_SOFTWARE);
	}
	// printf("input file descriptor = %d\n", input_fd);
	if (input_fd < 0) specify_os_error_and_exit();

	RowLayout row_lo = {0};
	// allocating some memory
	{ // NOTE: could be it's own func
		RowInfo info = {0};
		// printf("Allocating some memory\n");
		// memory location with info.string

		{ // isolating the err variable to not clutter scope
			int err = identify_L1(&info, input_fd);
			if (err) die("Failure parsing line 1 of the input file", err);
		}

		if (init_RowLayout(&row_lo, &info, &conf)) {
			die("Inconclusive eol configuration and detection", EX_DATAERR);
		}
	}

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
	printf("getting input file statistics\n");
	struct stat st;
	if (fstat(input_fd, &st)) die("could not read source file stats", EX_SOFTWARE);

	off_t file_size = st.st_size;
	// printf("Input file size: \n");
	// print_size_info(file_size);

	// counts the rank of the last processed row of tiles
	int tile_row = 0;

	// offset between the begining of the file and the current read position.
	off_t true_offset = 0;
	// true_offset rounded down to the nearest multiple of pagesize
	off_t file_offset = 0;
	// relative offset = true_offset - file_offset -> Within 0 and pagesize
	off_t relative_offset = 0;

	/*
	 *=========================== Processing phase ============================
	 */

	printf("Setup finished, starting processing\n");

	char INPUT_READING_COMPLETE = 0;

	char FULLFILE_FAILED = 0;

	// We don't know the number of rows in advance so no for loop
	while(!INPUT_READING_COMPLETE) {
		printf("processing chunk [%d]\n", tile_row);
		// map input file to memory
		errno = 0;
		// PERF: could be optimized by using the MAP_FIXED flag?
		rdbuff.start = mmap(NULL, rdbuff.bytesize, PROT_READ, MAP_PRIVATE|MAP_FILE, input_fd, file_offset);

		if (rdbuff.start == MAP_FAILED) {
			char msg[SMALL_ERR_MSG_SIZE] = {0};
			int err = handle_mmap_error(errno, msg, SMALL_ERR_MSG_SIZE);
			die(msg, err);
		}

		printf("file successfully mapped to memory [%d]\n", tile_row);


		int read_rows = read_chunk(
			&rdbuff, &cpbuff, &row_lo,
			&relative_offset, &true_offset, &file_offset, file_size,
			&INPUT_READING_COMPLETE
		);

		printf("data successfully converted to float [%d]\n", tile_row);

		munmap(rdbuff.start, rdbuff.bytesize); // size == byte_size since sizeof(char) == 1

		// comp
		// Only compute as much as was parsed
		if (INPUT_READING_COMPLETE) {
			// calc write buff row count again
			printf("last chunk reached [%d]\n", tile_row);
			pvbuff.row_count = read_rows >> 2;
			pvbuff.bytesize = pvbuff.row_count * pvbuff.row_length;
		}

		pvbuff.start = (float *) malloc(pvbuff.bytesize);
		if (pvbuff.start == NULL) die("Out of Memory (malloc pvbuff).", EX_OSERR);

		subsample(&cpbuff, &pvbuff);
		
		printf("subsampling finished [%d]\n", tile_row);

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

		printf("filling file buffers [%d]\n", tile_row);
		fill_filebuffers(&pvbuff, &wrbuff);
		fill_fullfile_buffer(&ffbuff, &wrbuff);

		printf("writing to files [%d]\n", tile_row);
		write_buffers_to_files(&wrbuff, &conf, tile_row);
		if (!FULLFILE_FAILED) {
			FULLFILE_FAILED = write_FullFileBuffer_to_file(&ffbuff, &conf);
		}


		free(pvbuff.start);
		free(wrbuff.file_buffers);
		free(wrbuff.buffer);
		free(ffbuff.buffer);
		printf("chunk processed [%d]\n", tile_row);

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
