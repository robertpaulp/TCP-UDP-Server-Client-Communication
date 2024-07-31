#include <sys/socket.h>
#include <sys/types.h>

#include "libserver.hpp"

using namespace std;

bool search_client_id_table(struct client_id_table *table, int len, char *id) {

    for (int i = 0; i < len; i++) {
        if (strncmp(table[i].id, id, strlen(id)) == 0) {
            return true;
        }   
    }

    return false;
}

void add_client_id_table_entry(struct client_id_table *table, int *len, char *id, int socketfd) {

    strcpy(table[*len].id, id);
    table[*len].socket_fd = socketfd;

    (*len)++;
}

char *get_client_id_table_entry(struct client_id_table *table, int len, int socketfd) {

    for (int i = 0; i < len; i++) {
        if (table[i].socket_fd == socketfd) {
            return table[i].id;
        }
    }

    return NULL;
}

void remove_client_id_table_entry(struct client_id_table *table, int *len, int socketfd) {
    
    for (int i = 0; i < *len; i++) {
        if (table[i].socket_fd == socketfd) {
            memset(table[i].id, 0, sizeof(table[i].id));
            for (int j = i; j < *len - 1; j++) {
                memcpy(table[j].id, table[j + 1].id, sizeof(table[j + 1].id));
                table[j].socket_fd = table[j + 1].socket_fd;
            }
            (*len)--;
            break;
        }
    }
}

bool wildcard_pattern_match(char *text, char *pattern) {
    int text_len = strlen(text);
    int pattern_len = strlen(pattern);
    int text_idx = 0, pattern_idx = 0;
    int last_star_index = -1, last_match_index = 0;

    while (text_idx < text_len) {
        if (pattern_idx < pattern_len && (pattern[pattern_idx] == text[text_idx] || pattern[pattern_idx] == '?')) {

            text_idx++;
            pattern_idx++;

        } else if (pattern_idx < pattern_len && pattern[pattern_idx] == '+') {

            while (text_idx < text_len && text[text_idx] != '/') {
                text_idx++;
            }
            pattern_idx++;

        } else if (pattern_idx < pattern_len && pattern[pattern_idx] == '*') {

            last_star_index = pattern_idx;
            last_match_index = text_idx;
            pattern_idx++;

        } else if (last_star_index != -1) {

            pattern_idx = last_star_index + 1;
            last_match_index++;
            text_idx = last_match_index;
            
        } else {
            return false;
        }
    }

    return pattern_idx == pattern_len;
}