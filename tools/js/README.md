Loading PT Database
===================
##### Live Demo

 - [DEMO](https://easz.github.io/procrastitracker/tools/js/demo.html)

##### Usage

```js
{
  var data; // load PT file in a Uint8Array
  var compressed = new Zlib.Gunzip(data); // unzip
  var decompressed = compressed.decompress();
  var json = PROCRASTITRACKER.load_db(decompressed); // loaded!
}
```

##### Required libs

  - Zlib [zlib.js](https://github.com/imaya/zlib.js/)

##### Notes

  - Only tested with the latest PT format (i.e. FILE_FORMAT_VERSION = 10)
  - Following the format https://github.com/aardappel/procrastitracker/blob/master/PT/file_format.txt
    - Output json data has the same structure as original PT file (e.g. without accumulation and filtering)
    - Datetime object is stored in a human readable way (e.g. ISO 8601)
  - Not completely verified yet. The result may not be absolutely correct.
