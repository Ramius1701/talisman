local userhandle;
local onelinerfile;

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

onelinerfile = bbs_get_data_path() .. "/oneliners.dat"

while true do
	bbs_clear_screen();
	bbs_display_gfile("oneliners")

	local lines = lines_from(onelinerfile)
	for i = 1, 12 do
		if i <= #lines then
			bbs_write_string(lines[i] .. "\r\n")
		end
	end

	bbs_write_string("\r\n")
	bbs_write_string("|14Add your thoughts ? (Y / N):|07")

	local command = bbs_getchar()
	if command == "y" or command == "Y" then
		bbs_write_string("\r\n|14Your Line: |07")
		local tmpline = bbs_read_string(60)

		if string.len(tmpline) > 0 then
			local file = io.open(onelinerfile, "w")
			local start = 1

			if #lines == 12 then
				start = 2
			end

			for i = start, #lines do
				file:write(lines[i] .. "\n")
			end

			file:write("|07" .. tmpline .. " |08 -> |15 " .. bbs_get_username() .. "\n")
			file:close()
		end
	else
		return
	end
end
