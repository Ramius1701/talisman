local function readLines(sPath)
    local file = io.open(sPath, "r")
    if file then
          local tLines = {}
          local sLine = file:read()
          while sLine do
            table.insert(tLines, sLine)
            sLine = file:read()
          end
          file:close()
          return tLines
    end
    return nil
end

local tLines = readLines(bbs_get_data_path() .. "/rumors.dat");

if (tLines == nil or #tLines == 0) then
    bbs_write_string("No rumours here!");
else 
    local rand = math.random(#tLines);
    bbs_write_string(tLines[rand]);
end