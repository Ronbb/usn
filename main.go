package main

import (
	"encoding/base64"
	"errors"
	"fmt"
	"time"

	"github.com/ronbb/usn/usn"
)

func main() {
	err := enumUsnRecord()
	if err != nil {
		println(err.Error())
	}
}

func enumUsnRecord() error {
	p := "E:\\"
	volume := usn.Volume(p)
	isNTFS, err := usn.IsNTFS(volume)
	if err != nil {
		return err
	}
	if !isNTFS {
		return errors.New("is not ntfs")
	}

	handle, err := usn.NewHandle(volume)
	if err != nil {
		return err
	}
	println(volume)

	err = usn.CreateJournal(handle)
	if err != nil {
		return err
	}

	queryData, err := usn.QueryJournal(handle)
	if err != nil {
		return err
	}
	startUSN := queryData.FirstUsn
	endUSN := queryData.NextUsn
	d, err := usn.EnumJournal(handle, startUSN, endUSN)
	if err != nil {
		return err
	}

	println(1, len(d))

	c := d["ircEAAAAAwAAAAAAAAAAAA=="]
	found := false

	for {
		println(c.FileName, c.FileReference, c.ParentFileReference)
		p := c.ParentFileReference
		c, found = d[p]
		if !found {
			b, err := base64.StdEncoding.DecodeString(p)
			fmt.Println("root", b, err)
			break
		}
	}

	<-time.NewTimer(10 * time.Second).C

	queryData, err = usn.QueryJournal(handle)
	if err != nil {
		return err
	}

	if queryData.NextUsn > endUSN {
		startUSN = endUSN + 1
		endUSN = queryData.NextUsn
	}

	d, err = usn.EnumJournal(handle, startUSN, endUSN)

	if err != nil {
		return err
	}

	println(2, len(d))

	err = usn.DeleteJournal(handle, queryData.UsnJournalID)
	if err != nil {
		return err
	}

	return nil
}

// func getUsn() *usn.Journal {
// 	j, err := usn.NewJournal("D:\\TempCfg")
// 	if err != nil {
// 		println(err)
// 		return nil
// 	}

// 	println(volumeapi.MountPoint("D:\\TempCfg"))
// 	d, err := j.Query()
// 	if err != nil {
// 		println(err)
// 		return nil
// 	}

// 	usn.CreateJournal()

// 	return j
// }
