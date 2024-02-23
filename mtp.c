#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE 50

struct string{
  char* line;
  ssize_t len;
};


// Define buffers and variables
char* buffer1[BUFFER_SIZE];
ssize_t line_len1[BUFFER_SIZE];
int buffer_count1 = 0;
int producer_index1 = 0;
int consumer_index1 = 0;

char* buffer2[BUFFER_SIZE];
ssize_t line_len2[BUFFER_SIZE];
int buffer_count2 = 0;
int producer_index2 = 0;
int consumer_index2 = 0;

char* buffer3[BUFFER_SIZE];
ssize_t line_len3[BUFFER_SIZE];
int buffer_count3 = 0;
int producer_index3 = 0;
int consumer_index3 = 0;

// Mutexes for buffers 
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;

// Conditions for buffers
pthread_cond_t buffer1_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer2_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer3_full = PTHREAD_COND_INITIALIZER;

// Stop Flag & Length of Flag
static char* STOP_FLAG = "STOP\n";
static ssize_t STOP_LEN = 5;

ssize_t
get_user_input(char** input) {
  size_t n = 0;
  ssize_t len = getline(input, &n, stdin);
  if (len == -1) {
    if (feof(stdin)) return -1;
    else err(1, "stdin");
  }
  if (strcmp(*input, STOP_FLAG) == 0) return -1;
  return len;
}

// Put line into buffer1
void put_buff1(char* line, ssize_t len) {
  pthread_mutex_lock(&mutex1);
  buffer1[producer_index1] = line;
  line_len1[producer_index1] = len;
  buffer_count1++;
  pthread_cond_signal(&buffer1_full);
  pthread_mutex_unlock(&mutex1);
  producer_index1++;
}

void* input_thread(void* args) {
  char* line = NULL;
  ssize_t len = 0;
  while (len != -1) {
    // getline with STOP_FLAG
    len = get_user_input(&line);
    put_buff1(line, len);
  }
  // if get_user_input is -1, put stop flag
  put_buff1(STOP_FLAG, STOP_LEN);
  return NULL;
}

// Get line from buffer1
struct string* get_buff1() {
  pthread_mutex_lock(&mutex1);
  // Wait for buffer to fill
  while (buffer_count1 == 0) {
    pthread_cond_wait(&buffer1_full, &mutex1);
  }
  char* line = buffer1[consumer_index1];
  ssize_t len = line_len1[consumer_index1];
  buffer_count1--;
  pthread_mutex_unlock(&mutex1);
  consumer_index1++;
  struct string* output = calloc(1, sizeof(struct string));
  output->line = strdup(line);
  output->len = len;
  return output;
}

// Put line without line separators into buffer 2
void put_buff2(char* line, ssize_t len) {
  pthread_mutex_lock(&mutex2);
  buffer2[producer_index2] = line;
  line_len2[producer_index2] = len;
  buffer_count2++;
  pthread_cond_signal(&buffer2_full);
  pthread_mutex_unlock(&mutex2);
  producer_index2++;
}

// Removes line seperators
void* line_separator_thread(void* args) {
  char* line = "";
  ssize_t len;
  while (strcmp(line, STOP_FLAG) != 0) {
    struct string* string = get_buff1();
    line = strdup(string->line);
    len = string->len;
    if (strcmp(line, STOP_FLAG) == 0) {
      put_buff2(STOP_FLAG, STOP_LEN);
      return NULL;
    }
    if (line[len - 1] == '\n') {
      line[len - 1] = ' ';
    }
    put_buff2(line, len);
  }
  return NULL;
}

struct string* get_buff2() {
  pthread_mutex_lock(&mutex2);
  // when buffer is empty, wait
  while (buffer_count2 == 0) {
    pthread_cond_wait(&buffer2_full, &mutex2);
  }
  char* line = buffer2[consumer_index2];
  ssize_t len = line_len2[consumer_index2];
  buffer_count2--;
  pthread_mutex_unlock(&mutex2);
  consumer_index2++;
  struct string* output = calloc(1, sizeof(struct string));
  output->line = line;
  output->len = len;
  return output;
}

// Put line with replaced '^' into buffer3
void put_buff3(char* line, ssize_t len) {
  pthread_mutex_lock(&mutex3);
  buffer3[producer_index3] = line;
  line_len3[producer_index3] = len;
  buffer_count3++;
  pthread_cond_signal(&buffer3_full);
  pthread_mutex_unlock(&mutex3);
  producer_index3++; 
}

// Changes '++' to '^' from buffer 2
void* plus_sign_thread(void* args) {
  char* line = "";
  ssize_t len = 0;
  while (strcmp(line, STOP_FLAG) != 0) {
    struct string* string = get_buff2();
    line = string->line;
    len = string->len;
    char* new_line = malloc(len * sizeof(char));
    ssize_t new_line_len = 0;
    for (ssize_t i = 0; i < len; ++i) {
      if (line[i] == '+' && line[i + 1] == '+') {
        new_line[new_line_len++] = '^';
        i++;
      }
      else {
        new_line[new_line_len++] = line[i];
      }
    }
    new_line = realloc(new_line, new_line_len);
    put_buff3(new_line, new_line_len);
  }
  return NULL;
}

struct string* get_buff3() {
  pthread_mutex_lock(&mutex3);
  // When buffer is empty, wait
  while (buffer_count3 == 0) {
    pthread_cond_wait(&buffer3_full, &mutex3);
  }
  char* line = buffer3[consumer_index3];
  ssize_t len = line_len3[consumer_index3];
  buffer_count3--;
  pthread_mutex_unlock(&mutex3);
  consumer_index3++;
  // Create string struct to return both string data and length
  struct string* output = calloc(1, sizeof(struct string));
  output->line = line;
  output->len = len;
  return output;
}

void* output_thread(void* args) {
  struct string* string = get_buff3();
  char* line = string->line;
  ssize_t len = string->len;
  char output[81];
  // every 80 char needs \n to end it
  output[80] = '\n';
  int out_index = 0;
  while (strcmp(line, STOP_FLAG) != 0) {
    // Keeps track of how much of line has been read
    int i = 0;
    
    add_more:;

    // Need to reuse i if we jump back to add_more:
    while (i < len && out_index < 80) {
      output[out_index] = line[i];
      out_index++;
      i++;
    }
    // Only print 80 characters from the buffer and add \n
    if (out_index == 80) {
      fflush(stdout);
      write(1, output, 81);
      out_index = 0;
      /* 
       * since the line from the buffer has not been used
       * up yet, go back and fill output more
       */
      if (i < len) goto add_more;
    }
    string = get_buff3();
    line = string->line;
    len = string->len;
  }
  return NULL;
}


int main(void)
{
  pthread_t input_tid, line_sep_tid, plus_sign_tid, output_tid;
  pthread_create(&input_tid, NULL, input_thread, NULL);
  pthread_create(&line_sep_tid, NULL, line_separator_thread, NULL);
  pthread_create(&plus_sign_tid, NULL, plus_sign_thread, NULL);
  pthread_create(&output_tid, NULL, output_thread, NULL);

  pthread_join(input_tid, NULL);
  pthread_join(line_sep_tid, NULL);
  pthread_join(plus_sign_tid, NULL); 
  pthread_join(output_tid, NULL);
  return EXIT_SUCCESS;
}

