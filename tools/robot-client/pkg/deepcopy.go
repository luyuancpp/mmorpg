package pkg

import (
	"bytes"
	"encoding/gob"
)

func GobDeepCopy(src interface{}) (interface{}, error) {
	buf := new(bytes.Buffer)
	enc := gob.NewEncoder(buf)
	err := enc.Encode(src)
	if err != nil {
		return nil, err
	}
	dec := gob.NewDecoder(buf)
	var dest interface{}
	err = dec.Decode(&dest)
	if err != nil {
		return nil, err
	}
	return dest, nil
}
