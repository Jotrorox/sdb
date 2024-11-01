# Simple Database (SDB)

A lightweight, file-based key-value database library written in C.
It is not meant to be a full-featured database, but rather a simple way to store data.
Also it is single-threaded and not meant to be used in a multi-threaded environment.
And written in pure C with minimal dependencies.

**don't use it in production (yet)**

## Features

- Simple key-value storage
- Table-based organization
- Persistent storage to disk
- Easy to integrate
- Written in pure C with minimal dependencies

## Usage

To use the library, include the `sdb.h` header file in your project 

```c
#include "sdb.h"
```

# Example

```c
#include "sdb.h"

int main() {
    SDB* sdb = sdb_open("test.sdb");
    if (sdb == NULL) {
        printf("Failed to open database\n");
        return 1;
    }

    sdb_table_create(sdb, "test");
    sdb_table_set(sdb, "test", "key", "value");
    sdb_table_set(sdb, "test", "key2", "value2");

    sdb_close(sdb);
}
```

# Contributing

Contributions are welcome! Please open an issue or submit a pull request.

## Contribution Guidelines

- Please open an issue before submitting a pull request.
- Please follow the code style of the file you are editing.
- Please add yourself to the list of authors if you contribute to the project.

## Authors

- Johannes (Jotrorox) Müller

# License

This project is licensed under the AGPL-3.0-or-later License. See the [LICENSE](LICENSE) file for details.

# Contact

For questions or suggestions, please contact me via email at mail@jotrorox.com.
Or open an issue on [GitHub](https://github.com/Jotrorox/sdb/issues).
