-- Set these to the conference / area you want to post to --
local msg_area = "fsx/dat"
local sysopname = "apam"
local bbs_name = "Talisman BBS"
local bbs_address = "talismanbbs.com:11892";
------------------------------------------------------------

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


local userhandle = bbs_get_userhandle();
local userlocation = bbs_get_user_location();
local thetime = os.time();
local sysname = bbs_get_os();
local current_date = os.date("%d/%m/%y", thetime);
local current_time = string.sub(os.date("%I:%M%p"), 0, 6);



if userhandle ~= sysopname then
    local messagebody = ">>> BEGIN\r" .. rot47_cipher(userhandle) .. "\r" .. rot47_cipher(bbs_name) .. "\r" .. rot47_cipher(current_date) .. "\r" .. rot47_cipher(current_time) .. "\r" .. rot47_cipher(userlocation) .. "\r" .. rot47_cipher(sysname) .. "\r" .. rot47_cipher(bbs_address) .. "\r>>> END\r";
    bbs_post_message(msg_area, "All", "ibbslastcall", "ibbslastcall-data", messagebody);
end
