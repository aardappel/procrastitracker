Loading PT Database
===================

##### Example

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