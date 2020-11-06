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



while true do
    local lines = lines_from(bbs_get_data_path() .. "/rumors.dat");
        
    bbs_clear_screen();
    bbs_display_gfile("rumors");

    local start = 1;

    if (#lines > 12) then
        start = #lines - 12;
    end

    for i = start, #lines do
        bbs_write_string("|07" .. lines[i] .. "\r\n");
    end

    bbs_write_string("\r\n|15A|08=|14Add a Rumor |15V|08=|14View all rumors |15Q|08=|14Quit\r\n");

    local command = bbs_getchar();

    if (command == "a" or command == "A") then
        bbs_write_string("\r\n\r\n|14Your Rumor: |07");
        local tmpline = bbs_read_string(60);
        if (string.len(tmpline) > 0) then
            local file = io.open(bbs_get_data_path() .. "/rumors.dat", "a");
            file:write(tmpline .. "\n");
            file:close();
        end
    elseif (command == "v" or command == "V") then
        bbs_clear_screen();
        for i = 1, #lines do
            bbs_write_string(lines[i] .. "\r\n");
            if (i > 1 and i % 23 == 0) then
                bbs_write_string("|14Press any key to continue...|07");
                bbs_getchar();
            end
        end
        bbs_write_string("|14Press any key to continue...|07");
        bbs_getchar();        
    elseif (command == "q" or command == "Q") then
        return;
    end
end
