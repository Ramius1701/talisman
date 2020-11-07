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


local tLines = lines_from(bbs_get_data_path() .. "/automessage.dat");

if (tLines == nil or #tLines == 0) then
  bbs_write_string("|14No automessage has been left!");
else 
  for i = 1, #tLines do
    bbs_write_string(tLines[i]);
    if (i < #tLines) then
      bbs_write_string("\r\n");
    end
  end
end