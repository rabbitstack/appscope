#include <fcntl.h>

#include "test_utils.h"

int do_test() {
    int test_result = EXIT_SUCCESS;
    char tmp_file_name[255];    
    int i = 0;

    CREATE_TMP_DIR();    
    
    sprintf(tmp_file_name, "%s/file", tmp_dir_name);

    for(i = 0; i < 100; i++) {
        char file_name[255];    
    	sprintf(file_name, "%s%d", tmp_file_name, i);
    	
        int fd = creat64(file_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
        if(fd != EOF ) {
            if(close(fd) == EOF) {
                TEST_ERROR();
            }
            unlink(file_name);
        } else {
            TEST_ERROR();
            break;
        }
    }
    
    REMOVE_TMP_DIR();
        
    return test_result;
}