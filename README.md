# RyanNet

A two file c library that can do cross platform network communications with an emphasis on game development.

## Getting Started

This project is very simple you could just grab the file and build them all manualy if you wanted.

### Prerequisites

You will want the build system bam. That is how I build it, you can download from here https://github.com/matricks/bam

#### Windows

Have the latest windows c compiler. Windows doesn't make this easy anymore.

#### Linux

You will want gcc.

### Building

Grab the repository

For windows you will want to run SetupWindowsCompiler.bat

To build

```
bam
```

To clean

```
bam -c
```


## Running the tests

There currently isn't really any go way to test. The project builds a ryannet_text exe that you can run. It will do some local tests.


## Contributing

You are welcome to reach out to me or make a pull request. Don't be a jerk. 

## Authors

* Ryan Hanson

## License

This project is licensed under the Zlib License - see the [LICENSE](LICENSE) file for details

## Acknowledgments

* beej Guide http://beej.us/guide/bgnet/

