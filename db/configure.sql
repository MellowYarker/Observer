CREATE TABLE keys(
    privkey VARCHAR(32) PRIMARY KEY,
    seed VARCHAR(32),
    P2PKH VARCHAR(34),
    P2SH VARCHAR(34),
    P2WPKH VARCHAR(34)
);

CREATE TABLE usedAddresses(
    address VARCHAR(48) PRIMARY KEY
);

CREATE TABLE spendable(
    address VARCHAR(48),
    -- Max script size:
    -- https://github.com/bitcoin/bitcoin/blob/v0.17.0/src/script/script.h#L31-L32
    script VARCHAR(10000),
    value UNSIGNED INTEGER,
    PRIMARY KEY (address, script)
);

/*We will probably want 2 more tables.
    1. Moved Funds
    2. Secure Keys
*/
