package main

import (
	"syscall"

	"golang.org/x/sys/windows"
)


func main() {
	println("hello ronbb")
	enumUsnRecord()
}

func enumUsnRecord() {
	var maximumComponentLength uint32
	fileSystemNameBuffer := [1000]uint16{}
	err := windows.GetVolumeInformation(syscall.StringToUTF16Ptr("C:\\"), nil, 0, nil, &maximumComponentLength, nil, &fileSystemNameBuffer[0], 1000)
	if err != nil {
		print(err)
	}
	println(syscall.UTF16ToString(fileSystemNameBuffer[:]))
}
