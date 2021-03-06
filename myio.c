//calder birdsey
//cs315 systems programming 
//assignment 2: myio 

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myio.h"

struct file_struct *
myopen(const char *pathname, int flags) {
    //initialize struct
    struct file_struct *bufdata = malloc(sizeof(*bufdata));  
    if(bufdata == NULL) {
        return NULL; 
    }
    //initialize rd/wr buffers
    bufdata->wr_bytes = 0;
    bufdata->rd_bytes = 0;
    bufdata->rdbuf_count = 0; 
    bufdata->rdwr_flag = 0; //used to tell what action was last done on this fd 
    bufdata->rdwr_off = 0; 
    bufdata->cur_off = 0; 

    //call open(2)
    bufdata->fd = open(pathname, flags, 0666);

    //check for open(2) error
    if(bufdata->fd == -1) {
        free(bufdata);
        return NULL; 
    }
    return bufdata;
}

int
myclose(struct file_struct *bufdata) {
    //flush file if necessary 
    if (bufdata->wr_bytes != 0) {
        myflush(bufdata); 
    }

    //close file 
    int close_status = close(bufdata->fd);  

    //check for close(2) error
    if(close_status == -1) { 
        return close_status; 
    }
    free(bufdata);
    return close_status; 
}

ssize_t
myread(struct file_struct *bufdata, void *trg_buf, size_t count) {
    //condition to check if mywrite was called prior to read call (RDWR case)
    if(bufdata->rdwr_flag == 2) {
        bufdata->rd_bytes = 0; 
        bufdata->rdbuf_count = 0; 
        myflush(bufdata); 
    } else {
        lseek(bufdata->fd, bufdata->rdwr_off, SEEK_SET); 
    }
    bufdata->rdwr_flag = 1; 
    int rd_bytes = bufdata->rd_bytes; 
    int rdbuf_count = bufdata->rdbuf_count; 
    int offset = rdbuf_count-rd_bytes; 
    if(count > BLOCK_SIZE) { 
        memcpy(trg_buf, bufdata->rd_buf + offset, rd_bytes);  
        char *trg_ptr = (char *)trg_buf + rd_bytes;
        int amt = read(bufdata->fd, trg_ptr, count - rd_bytes);
        bufdata->rdwr_off = lseek(bufdata->fd, 0, SEEK_CUR); 
        bufdata->rd_bytes = 0; 
        return amt;
    }
    if(rd_bytes < count) {
        if(rd_bytes != 0) {
            memcpy(bufdata->rd_buf, bufdata->rd_buf + offset, rd_bytes); 
        } 
        bufdata->cur_off = lseek(bufdata->fd, 0, SEEK_CUR); 
        rdbuf_count = read(bufdata->fd, bufdata->rd_buf + rd_bytes, (BLOCK_SIZE-rd_bytes)); 
        bufdata->rdwr_off = lseek(bufdata->fd, 0, SEEK_CUR); 
        lseek(bufdata->fd, bufdata->cur_off, SEEK_SET); 
        //check for read(2) error
        if(rdbuf_count == -1) {
            bufdata->cur_off = lseek(bufdata->fd, rdbuf_count, SEEK_CUR); 
            return rdbuf_count; 
        } 
        rdbuf_count += rd_bytes; 
        rd_bytes = rdbuf_count;
    }
    offset = rdbuf_count-rd_bytes;
    memcpy(trg_buf, bufdata->rd_buf + offset, count); 
    if (rd_bytes >= count) {
        rd_bytes -= count; 
        bufdata->rd_bytes = rd_bytes;  
        bufdata->rdbuf_count = rdbuf_count; 
        bufdata->cur_off = lseek(bufdata->fd, count, SEEK_CUR); 
        return count; 
    }
    bufdata->rd_bytes = 0;  
    bufdata->rdbuf_count = rdbuf_count;
    bufdata->cur_off = lseek(bufdata->fd, rdbuf_count, SEEK_CUR); 
    return rdbuf_count; 
}

ssize_t
mywrite(struct file_struct *bufdata, void *source_buf, size_t count) {
    if(bufdata->rdwr_flag == 1) {
        lseek(bufdata->fd, bufdata->cur_off, SEEK_SET); 
    }
    bufdata->rdwr_flag = 2; 
    int bytes_loaded = bufdata->wr_bytes; 
    int null_bytes = BLOCK_SIZE-bytes_loaded; 
    int bytes_written; 
    if(count > BLOCK_SIZE){ 
        write(bufdata->fd, bufdata->wr_buf, bytes_loaded);
        bufdata->wr_bytes = 0; 
        return write(bufdata->fd, source_buf, count); 
    }
    if(null_bytes < count) {
        bytes_written = write(bufdata->fd, bufdata->wr_buf, bytes_loaded);
        //check for write(2) error
        if(bytes_written == -1) {
            return bytes_written;  
        }  
        bytes_loaded = 0; 
    }
    memcpy((bufdata->wr_buf) + bytes_loaded, source_buf, count); 
    bytes_loaded += count; 
    bufdata->wr_bytes = bytes_loaded; 
    return count; 
}

off_t
myseek(struct file_struct *bufdata_in, struct file_struct *bufdata_out, off_t offset, int whence) {
    int bytes_unread = bufdata_in->rd_bytes; 
    if((offset < bytes_unread) && (whence == SEEK_CUR)) {
        bytes_unread -= offset;
    } else {
        if(whence == SEEK_SET) {
            lseek(bufdata_in-> fd, offset, whence); 
        } else {
            lseek(bufdata_in-> fd, offset - bytes_unread, whence); 
        }
        bytes_unread = 0; 
    }
    bufdata_in->rd_bytes = bytes_unread; 
    return offset; 
}

void
myflush(struct file_struct *bufdata) {
    write(bufdata->fd, bufdata->wr_buf, bufdata->wr_bytes);
    bufdata->wr_bytes = 0; 
}