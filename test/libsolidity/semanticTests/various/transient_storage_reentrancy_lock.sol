contract C {
    bool transient locked;
    modifier nonReentrant {
        require(!locked);
        locked = true;
        _;
        locked = false;
    }

    function test(address newAddress, bool reentrancy) nonReentrant public {
        if (reentrancy)
            reentrantCall(newAddress);
    }

    function reentrantCall(address a) public {
        this.test(a, false);
    }
}
// ====
// EVMVersion: >=cancun
// compileViaYul: false
// ----
// test(address,bool): 0x1234abcd, true -> FAILURE
// test(address,bool): 0x1234abcd, false ->
