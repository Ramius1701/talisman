function file_exists(file)
    local f = io.open(file, "rb")
    if f then f:close() end
    return f ~= nil
end
  
function lines_from(file)
    if not file_exists(file) then return {} end
    lines = {}
    for line in io.lines(file) do
      lines[#lines + 1] = line
    end
    return lines
end

function is_new_user()
    local usertime = bbs_get_user_attribute("first_on", "0")
    local curtime = os.time(os.date("!*t"))

    if curtime - usertime < 604800 then
        -- user is new
        return true
    end
    return false
end

local newusers = lines_from(bbs_get_data_path() .. "/newusers.dat")
local newusers_usernames = {}
local newusers_firston = {}
local found = false
local isnew = is_new_user()
local myname = bbs_get_username()

for i=1, #newusers do
    if i % 2 == 0 then
        if isnew == true and newusers[i] == myname then
            found = true
        end
        newusers_usernames[#newusers_usernames + 1] = newusers[i]
    else
        newusers_firston[#newusers_firston + 1] = newusers[i] 
    end
end

if found == false and isnew == true then
    newusers_usernames[#newusers_usernames + 1] = myname
    newusers_firston[#newusers_firston + 1] = bbs_get_user_attribute("first_on", "0")

    -- save newusers
    local file = io.open(bbs_get_data_path() .. "/newusers.dat", "w")
    local start = 1
    if #newusers_usernames > 7 then
        start = 2
    end
    for i=start, #newusers_usernames do
        file:write(newusers_firston[i] .. "\n")
        file:write(newusers_usernames[i] .. "\n")
    end
    file:close()
end



while true do
    local oneliners = lines_from(bbs_get_data_path() .. "/newusers_comments.dat")
    bbs_clear_screen()
    bbs_display_gfile("newusers_hdr")
    local start = 0
    if #newusers_usernames > 7 then
        start = 1
    end

    for i=1, 7 do
        if (i + start > #newusers_usernames) then
            bbs_write_string("\r\n");
        else
            local d = os.date("*t", newusers_firston[i])
        
            bbs_write_string(string.format("    |10NEWUSER!    |14Welcome, |15%-16.16s   |14Signed up: |15%04d/%02d/%02d %02d:%02d\r\n", newusers_usernames[i], d['year'], d['month'], d['day'], d['hour'], d['min']))
        end
    end

    bbs_write_string("|08-----|11Comments|08------------------------------------------------------------------\r\n")
    for i=1, 7 do
        if i > #oneliners then
            bbs_write_string("\r\n")
        else
            bbs_write_string(oneliners[i] .. "\r\n")
        end
    end

    bbs_write_string("|14Add a comment ? (Y / N):|07")
    local c = bbs_getchar()

    if c == 'y' or c == 'Y' then
        bbs_write_string("\r\n|08           |----------------------------------------------------------|")
		bbs_write_string("\r\n|14Your Line: |07")
        local tmpline = bbs_read_string(60)
		if string.len(tmpline) > 0 then
			local file = io.open(bbs_get_data_path() .. "/newusers_comments.dat", "w")
			local start = 1

			if #oneliners == 8 then
				start = 2
			end

			for i = start, #oneliners do
				file:write(oneliners[i] .. "\n")
			end

			file:write(string.format("|07%-60.60s |08> |15%-16.16s\n", tmpline, bbs_get_username()))
			file:close()
		end
    else
        return
    end

end