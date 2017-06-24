var PROCRASTITRACKER = (function () {

  /** load uncompressed PT data 
   * @param pt_data uncompressed data in Uint8Array
   * @return PT data in json format
   */
  var load_db = function (pt_data) {

    // const 
    const FILE_FORMAT_VERSION = 10;
    const MAXTAGS = 15;
    const NUM_PREFS = 6;

    // result
    var pt_data_json = {};
    // seeker on pt_data
    var byte_read_pos = 0;

    // read 4 bytes little endian data as a int value
    var pack_int4 = function (data, start = 0) {
      return (data[start + 3] << 24) | (data[start + 2] << 16) | (data[start + 1] << 8) | (data[start + 0]);
    };
    // read 2 bytes little endian data as a short value
    var pack_short2 = function (data, start = 0) {
      return (data[start + 1] << 8) | (data[start + 0]);
    };
    // read the next 16 bytes little endian data and pack them as a SYSTEMTIME
    var rsystemtime = function () {
      var wYear = pack_short2(pt_data, byte_read_pos); byte_read_pos += 2; // forward seeker
      var wMonth = pack_short2(pt_data, byte_read_pos); byte_read_pos += 2; // forward seeker
      var wDayOfWeek = pack_short2(pt_data, byte_read_pos); byte_read_pos += 2; // forward seeker
      var wDay = pack_short2(pt_data, byte_read_pos); byte_read_pos += 2; // forward seeker
      var wHour = pack_short2(pt_data, byte_read_pos); byte_read_pos += 2; // forward seeker
      var wMinute = pack_short2(pt_data, byte_read_pos); byte_read_pos += 2; // forward seeker
      var wSecond = pack_short2(pt_data, byte_read_pos); byte_read_pos += 2; // forward seeker
      var wMilliseconds = pack_short2(pt_data, byte_read_pos); byte_read_pos += 2; // forward seeker
      return { 'wYear': wYear, 'wMonth': wMonth, 'wDayOfWeek': wDayOfWeek, 'wDay': wDay, 'wHour': wHour, 'wMinute': wMinute, 'wSecond': wSecond, 'wMilliseconds': wMilliseconds }
    };
    // read the next 2 bytes little endian data and pack them as a short
    var rshort = function () {
      var val = pack_short2(pt_data, byte_read_pos);
      byte_read_pos += 2; // forward seeker
      return val;
    };
    // read the next 4 bytes little endian data and pack them as a int
    var rint = function () {
      var val = pack_int4(pt_data, byte_read_pos);
      byte_read_pos += 4; // forward seeker
      return val;
    };
    // read the next 1 byte as char
    var rchar = function () {
      var char = pt_data[byte_read_pos];
      byte_read_pos++; // forward seeker
      return char;
    };
    // read the next fixed {size} bytes as null terminated string.
    // if {size} is 0, the actual size to read is automatically decided (i.e. null terminated)
    // by default the data is decoded as utf8 
    var rstr = function (size = 0, utf8 = true) {
      var str = "";
      if (size > 0) {
        for (var i = 0; i < size; i++) {
          var char = pt_data[byte_read_pos + i];
          if (char == 0)
            break;
          str += String.fromCharCode(char);
        }
        byte_read_pos += size; // forward seeker
      }
      else {
        while (byte_read_pos < pt_data.length) {
          var char = pt_data[byte_read_pos];
          byte_read_pos++; // forward seeker
          if (char == 0)
            break;
          str += String.fromCharCode(char);
        }
      }
      if (utf8)
        return decodeURIComponent(escape(str));
      return str;
    };

    // load meta
    const version = rint();
    pt_data_json['version'] = version;
    if (version > FILE_FORMAT_VERSION)
      throw "PT: trying to load db from newer version";
    const magic = rint();
    if ((version >= 6) && (magic != pack_int4([0x46, 0x46, 0x54, 0x50]))) /*check PTFF magic*/
      throw "PT: not a PT database file";
    if (version >= 4) {
      const numtags = rint();
      if (numtags > MAXTAGS)
        throw "PT: wrong number of tags in file"
      pt_data_json['tags'] = [];
      for (var i = 0; i < numtags; i++) {
        var tag = {};
        tag['name'] = rstr(32);
        tag['color'] = "#" + ("000000" + rint().toString(16)).slice(-6);
        pt_data_json['tags'][i] = tag;
      }
      if (version < 6)
        rstr(10);
      else
        pt_data_json['minfilter'] = rint() / 60;
      pt_data_json['foldlevel'] = rint();
      pt_data_json['prefs'] = [];
      if (version >= 6) {
        for (var i = 0; i < NUM_PREFS - 1; i++)
          pt_data_json['prefs'][i] = rint();
        if (version >= 10)
          pt_data_json['prefs'][NUM_PREFS - 1] = rint();
      }
      if (version == 8)
        rstr(24);
    }
    // load node
    var load_node = function () {
      var node = {};
      node['name'] = rstr();
      if (version <= 1)
        rint();
      if (version >= 3)
        node['tagindex'] = rint();
      if (version >= 7)
        node['ishidden'] = (rchar() != 0 ? 1 : 0);
      const numberofdays = rint();
      node['days'] = [];
      var format = function (n) {
        return ("00" + n).slice(-2);
      }
      for (var i = 0; i < numberofdays; i++) {
        var day = {};
        if (version < 5) {
          var st = rsystemtime();
          day['datetime'] = st.wYear.toString() + "-" + format(st.wMonth.toString()) + "-" + format(st.wDay.toString()) + "T" + format(st.wHour.toString()) + ":" + format(st.wMinute.toString()) + ":" + format(st.wSecond.toString());
        }
        else {
          const nday = rshort();
          const nminute = rshort();
          var wYear = (nday / 512) | 0;
          var wMonth = ((nday - wYear * 512) / 32) | 0;
          var wDay = nday - wYear * 512 - wMonth * 32;
          wYear += 2000;
          var wHour = (nminute / 60) | 0;
          var wMinute = nminute - wHour * 60;
          day['datetime'] = wYear.toString() + "-" + format(wMonth.toString()) + "-" + format(wDay.toString()) + "T" + format(wHour.toString()) + ":" + format(wMinute.toString()) + ":00";
        }
        day['activeseconds'] = rint();
        day['semiidleseconds'] = rint();
        day['key'] = rint();
        day['lmb'] = rint();
        day['rmb'] = rint();
        day['scrollwheel'] = rint();
        node['days'][i] = day;
      }
      const numchildren = rint();
      node['children'] = [];
      for (var i = 0; i < numchildren; i++) {
        node['children'][i] = load_node();
      }
      return node;
    }

    // load from the root node and recursively
    pt_data_json['root'] = load_node();

    if (pt_data.length != byte_read_pos)
      throw "PT file is not read completely";

    return pt_data_json;
  };

  var parse_node_datetime = function (node, result, 
                            stack = { name: [] }) {
    const name = node.name;
    const days = node.days;
    const children = node.children;

    /* push */
    stack.name.push(name);

    /* collect time */
    if (days.length > 0) {
      const path = ((stack.name.length > 1) ? stack.name[1] : stack.name[0]) 
        + ((stack.name.length > 2) ? (" " + stack.name.slice(2).join("/")) : "");
      for (var i = 0; i < days.length; i++) {
        var day = days[i];
        var datetime = day.datetime;
        var [date, time] = datetime.split("T");
        var activeseconds = day.activeseconds;
        var semiidleseconds = day.semiidleseconds;
        (date in result)             || (result[date]             = {});
        (time in result[date])       || (result[date][time]       = {});
        (path in result[date][time]) || (result[date][time][path] = {'activeseconds': 0, 'semiidleseconds': 0});
        result[date][time][path].activeseconds += activeseconds;
        result[date][time][path].semiidleseconds += semiidleseconds;
      }
    }

    // process children
    for (var i = 0; i < children.length; i++) {
      var child = children[i];
      parse_node_datetime(child, result, stack);
    }

    /* pop */
    stack.name.pop();
  };

  var parse_node_path = function (node, result, 
                            stack = { name: [] }) {
  };

  var parse_datetime = function (pt_data_json) {
    var root = pt_data_json.root;
    var result = {};
    parse_node_datetime(root, result);
    return result;
  };

  var parse_path = function (pt_data_json) {
    var root = pt_data_json.root;
    var result = {};
    parse_node_path(root, result);
    return result;
  };

  return {
    load_db: load_db,
    parse_datetime: parse_datetime,
    parse_path: parse_path
  }
})();
