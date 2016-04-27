#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <stdarg.h>


#define BUFFER_SIZE 10
uint32_t microseconds = 100;

/**
 * 线程信息
 */
typedef struct _thread_info{
    pthread_t thread_id;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int run_flag;
    int buffer_flag;
    int shutdown;
} thread_info;

// 线程函数参数
typedef struct _thread_arg {
    FILE *fp;
} thread_arg;


thread_info input_info, output_info, cal_input_info, cal_output_info;
int read_buffer_a[BUFFER_SIZE], read_buffer_b[BUFFER_SIZE];
int write_buffer_a[BUFFER_SIZE], write_buffer_b[BUFFER_SIZE];


void init_resources(int n, ...) {
    va_list arg_ptr ;
    int i;
    va_start(arg_ptr, n);
    thread_info * tmp_info = NULL;

    for(i = 0; i < n; i++) {
       tmp_info = va_arg(arg_ptr, thread_info *);
       pthread_mutex_init(&(tmp_info->lock), NULL);
       pthread_cond_init(&(tmp_info->cond), NULL);
    }

    va_end(arg_ptr);
}

void free_resources(int n, ...) {
    va_list arg_ptr;
    int i;
    va_start(arg_ptr, n);
    thread_info * tmp_info = NULL;
    for(i = 0; i < n; i++) {
        tmp_info = va_arg(arg_ptr, thread_info *);
        pthread_mutex_destroy(&(tmp_info->lock));
        pthread_cond_destroy(&(tmp_info->cond));
    }
    va_end(arg_ptr);
}

void * input_task(void * args){
    thread_arg * input_arg = (thread_arg *) args;
    int i;

    while(1) {
        pthread_mutex_lock(&(input_info.lock));
        while(input_info.run_flag == 0 && !input_info.shutdown) {
            pthread_cond_wait(&(input_info.cond), &(input_info.lock));
        }
        if(input_info.shutdown) {
            break;
        }
        input_info.run_flag = 0;
        input_info.buffer_flag = 1 - input_info.buffer_flag;
        pthread_mutex_unlock(&(input_info.lock));
        
        if(input_info.buffer_flag) {
            for(i = 0; i < BUFFER_SIZE; i++) {
                fscanf(input_arg->fp, "%d", read_buffer_a + i);
            }
        } else {
            for(i = 0; i < BUFFER_SIZE; i++) {
                fscanf(input_arg->fp, "%d", read_buffer_b + i);
            }
        }

        pthread_mutex_lock(&(cal_input_info.lock));
        cal_input_info.run_flag = 1;
        pthread_cond_signal(&(cal_input_info.cond));
        pthread_mutex_unlock(&(cal_input_info.lock));
    }

    return NULL;
}

void * output_task(void * args){
    thread_arg * output_arg = (thread_arg *) args;
    int i;

    while(1) {
        pthread_mutex_lock(&(output_info.lock));
        while(output_info.run_flag == 0 && !output_info.shutdown) {
            pthread_cond_wait(&(output_info.cond), &(output_info.lock));
        }
        if(output_info.shutdown) {
            break;
        }
        output_info.run_flag = 0;
        output_info.buffer_flag = 1 - output_info.buffer_flag;
        pthread_mutex_unlock(&(output_info.lock));
        
        if(output_info.buffer_flag) {
            for(i = 0; i < BUFFER_SIZE; i++) {
                fprintf(output_arg->fp, "%d\n", write_buffer_a[i]);
                usleep(microseconds);
            }
        } else {
            for(i = 0; i < BUFFER_SIZE; i++) {
                fprintf(output_arg->fp, "%d\n", write_buffer_b[i]);
                usleep(microseconds);
            }
        }

        pthread_mutex_lock(&(cal_output_info.lock));
        cal_output_info.run_flag = 1;
        pthread_cond_signal(&(cal_output_info.cond));
        pthread_mutex_unlock(&(cal_output_info.lock));

    }
    return NULL;
}

int main(){
    FILE *fp_input, *fp_output;
    char *input_name = "input.txt";
    char *output_name = "output.txt";
    int total_nums = 100;
    int loop_nums = total_nums / BUFFER_SIZE;
    int loop_index = 0;
    int i;
    thread_arg input_arg, output_arg;

    if((fp_input = fopen(input_name, "r")) == NULL) {
        printf("can't load input file\n");
        exit(1);
    }

    if((fp_output = fopen(output_name, "w+")) == NULL) {
        printf("can't load output file\n");
        exit(1);
    }

    input_arg.fp = fp_input;
    output_arg.fp = fp_output;

    init_resources(4, &input_info, &output_info, &cal_input_info, &cal_output_info);
    input_info.buffer_flag = output_info.buffer_flag = cal_input_info.buffer_flag = 0;
    input_info.run_flag = cal_output_info.run_flag = 1;
    output_info.run_flag = cal_input_info.run_flag = 0;
    input_info.shutdown = output_info.shutdown = 0;

    pthread_create(&(input_info.thread_id), NULL, input_task, &input_arg);
    pthread_create(&(output_info.thread_id), NULL, output_task, &output_arg);
    
    while(1) {
        pthread_mutex_lock(&(cal_input_info.lock));
        while(cal_input_info.run_flag == 0) {
            pthread_cond_wait(&(cal_input_info.cond), &(cal_input_info.lock));
        }
        cal_input_info.buffer_flag = 1 - cal_input_info.buffer_flag;
        cal_input_info.run_flag = 0;
        pthread_mutex_unlock(&(cal_input_info.lock));

        pthread_mutex_lock(&(input_info.lock));
        if(loop_index == loop_nums - 1) {
            input_info.shutdown = 1;
        } 
        input_info.run_flag = 1;
        pthread_cond_signal(&(input_info.cond));
        pthread_mutex_unlock(&(input_info.lock));
        
        if(cal_input_info.buffer_flag) {
            for(i = 0; i < BUFFER_SIZE; i++) {
                write_buffer_a[i] = read_buffer_a[i] + 1;
            }
        } else {
            for(i = 0; i < BUFFER_SIZE; i++) {
                write_buffer_b[i] = read_buffer_b[i] + 1;
            }
        }
        
        pthread_mutex_lock(&(cal_output_info.lock));
        while(cal_output_info.run_flag == 0) {
            pthread_cond_wait(&(cal_output_info.cond), &(cal_output_info.lock));
        }
        cal_output_info.run_flag = 0;
        pthread_mutex_unlock(&(cal_output_info.lock));
        
        pthread_mutex_lock(&(output_info.lock));
        output_info.run_flag = 1;
        pthread_cond_signal(&(output_info.cond));
        pthread_mutex_unlock(&(output_info.lock));

        if(loop_index == loop_nums - 1) {
            break;
        }
        loop_index++;

    }
    pthread_mutex_lock(&(cal_output_info.lock));
    while(cal_output_info.run_flag == 0) {
        pthread_cond_wait(&(cal_output_info.cond), &(cal_output_info.lock));
    }
    cal_output_info.run_flag = 0;
    pthread_mutex_unlock(&(cal_output_info.lock));

    pthread_mutex_lock(&(output_info.lock));
    output_info.run_flag = 1;
    output_info.shutdown = 1;
    pthread_cond_signal(&(output_info.cond));
    pthread_mutex_unlock(&(output_info.lock));


    pthread_join(input_info.thread_id, NULL);
    pthread_join(output_info.thread_id, NULL);

    free_resources(4, &input_info, &output_info, &cal_input_info, &cal_output_info);
    fclose(fp_input);
    fclose(fp_output);
    

    return 0; 
}
