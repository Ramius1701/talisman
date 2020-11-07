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


bbs_clear_screen();
bbs_display_gfile("automessage");

local lines = lines_from(bbs_get_data_path() .. "/automessage.dat");

for i = 1, #lines do
	bbs_write_string("|07" .. lines[i] .. "\r\n");
end

bbs_write_string("|15E|08=|14Enter Message, |15ENTER|08=|14Quit |08: |07");

local command = bbs_getchar();
local newlines = {};

if (command == "e" or command == "E") then
	bbs_write_string("\r\n|14Enter your message, you have 5 lines...\r\n");
	for i = 1, 5 do
		bbs_write_string("\r\n|14".. i .. "|08: |07");
		newlines[i] = bbs_read_string("75");
	end
	bbs_write_string("\r\n|14Really save this message? (Y/N) : |07");
	command = bbs_getchar();
	if (command == "y" or command == "Y") then
		local file = io.open(bbs_get_data_path() .. "/automessage.dat", "w");
		file:write("|15" .. bbs_get_username() .. " |14left a message ...|07\n\n")
		for i = 1, 5 do
			file:write(newlines[i] .. " \n");
		end
		file:close();
	end
end
