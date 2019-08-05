#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bloom.h>

#include <btc.h>
#include <chainparams.h>
#include <ecc.h>
#include <ecc_key.h>

int main() {
    btc_ecc_start(); // load libbtc
    const btc_chainparams* chain = &btc_chainparams_main; // mainnet
    
    struct bloom address_bloom; // filter of all generated addresses.
    const char address_filter_file[] = "generated_addresses_filter.b";
    
    if (access((char *) &address_filter_file, F_OK) != -1) {
        if (bloom_load(&address_bloom, (char *) &address_filter_file) == 0) {
            printf("Loaded Address filter.\n");
        } else {
            printf("Failed to load bloom filter.\n");
        }
    } else {
        printf("Could not find filter: %s\n", address_filter_file);
    }

    // infinite loop goes here
    // read from socket, parse for outputs

    size_t sizeout = 128;
    char output_address[sizeout];
    strncpy(output_address, "1ExampLe_Address", 34);
    
    // here we loop over the Tx outputs

    // check if we own the output address
    if (bloom_check(&address_bloom, &output_address, strlen(output_address)) 
        == 1) {
        // we have to check the database
        printf("Check database.\n");
    }
    // otherwise keep going
    
    // end of loop via signal?
    bloom_free(&address_bloom);
    btc_ecc_stop();

    return 0;
}