CREATE TABLE keys(
    privkey VARCHAR(32) PRIMARY KEY,
    seed VARCHAR(32),
    P2PKH VARCHAR(34),
    P2SH VARCHAR(34),
    P2WPKH VARCHAR(34)
);

CREATE TABLE usedAddresses(
    address VARCHAR(34) PRIMARY KEY
);

/*We will probably want 2 more tables.
    1. Moved Funds
    2. Secure Keys
*/