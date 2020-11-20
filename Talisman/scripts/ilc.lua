-- Set these to the conference / area you want to post to --
local msg_area = "fsx/dat"
local sysopname = "apam"
local bbs_name = "Talisman BBS"
local bbs_address = "talismanbbs.com:11892";
------------------------------------------------------------

function file_exists(name)
    local f=io.open(name,"r")
    if f~=nil then io.close(f) return true else return false end
 end

-- ROT47 ciphering algorithm implementation
-- See: http://en.wikipedia.org/wiki/ROT13#Variants

-- Up-values to be used by the algorithm
local base = 33
local range = 126 - 33 + 1

-- Checks if the given char is convertible
-- ASCII Code should be within the range [33 .. 126]
local function rot47_convertible(char) 
  local v = char:byte()
  return v >= 33 and v <= 126 
end

-- Ciphering algorithm
local function cipher(str, key)
  return (str:gsub('.', function(s)
  if not rot47_convertible(s) then return s end
    return string.char(((s:byte() - base + key) % range) + base)
  end))
end

-- str     : a string to be ciphered
-- returns : the ciphered string
local function rot47_cipher(str) return cipher(str, 47)  end

-- str     : a string to be deciphered
-- returns : the deciphered string
local function rot47_decipher(str) return cipher(str, -47)  end

local lastreadfile = bbs_data_path() .. "ilc-lr.dat";
local file = io.open(lastreadfile, "r");
local lastread  = 0

if (file ~= nil) then
    io.input(file);
    lastread = io.read("*n");
    io.close();
end

local username = {};
local location = {};
local bbsname = {};
local udate = {};
local index = 0;
local i = 0;


local csvfile = bbs_data_path() .. "ilc.csv";

if file_exists(csvfile) then
    for line in io.lines(csvfile) do
        local data = {};
        data1, data2, data3, data4 = line:match("%s*(.-)|%s*(.-)|%s*(.-)|%s*(.-)$");
        username[index] = data1;
        location[index] = data2;
        bbsname[index] = data3;
        udate[index] = data4;
        index = index + 1;
    end
end

local nxt_msg = lastread + 1;

while (nxt_msg ~= 0) do 

    local sender;
    local recipient;
    local subject;
    local key = 0;
    local msgtext;

    nxt_msg, sender, recipient, subject, msgtext = bbs_get_message(msgarea, nxt_msg);

    if (nxt_msg ~= 0) then
        lastread = nxt_msg;
        nxt_msg = nxt_msg + 1;
    end

    if (sender == "ibbslastcall" and subject == "ibbslastcall-data") then
        local gotbegin = false;
        local lines = {};
        for line in string.gmatch(msgtext,'[^\r]+') do
            if line == ">>> END" then break end;
            if gotbegin == true then
                lines[key] = rot47_decipher(line);
                key = key + 1;
            end
            if line == ">>> BEGIN" then
                gotbegin = true;
            end
        end

        username[index] = lines[0];
        location[index] = lines[4];
        bbsname[index] = lines[1];
        udate[index] = lines[2];

        index = index + 1;
    end
end

file = io.open(csvfile, "w");
io.output(file);

local start = 0;

if (index > 15) then
    start = index - 15;
end

for i = start, index - 1 do
    io.write(username[i] .. "| " .. location[i] .. "| " .. bbsname[i] .. "| " .. udate[i] .. "\n");
end

io.close();

file = io.open(lastreadfile, "w");
io.output(file);
io.write(lastread);

io.close();

bbs_clear_screen();
bbs_write_string(" |08+----------------------------------------------------------------------------+|07\r\n");
bbs_write_string(" |08||20  |14>> |15Inter BBS Last Callers |14<<                                              |16|08||07\r\n");
bbs_write_string(" |08+----------------------------------------------------------------------------+|07\r\n");

for i = start, index - 1 do
    bbs_write_string(string.format("  |06m%-16s |01%-24s |05%-24s |03%-8s\r\n", username[i], location[i], bbsname[i], udate[i]));
end

bbs_write_string(" |08+----------------------------------------------------------------------------+|07\r\n");
bbs_write_string(" |14Press any key....\r\n\027|07");
bbs_read_char();
