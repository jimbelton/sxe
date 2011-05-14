do
  plan_tests(37)

  SXEL1("L1 - covered")
  SXEL2("L2 - covered")
  SXEL3("L3 - covered")
  SXEL4("L4 - covered")
  SXEL5("L5 - covered")
  SXEL6("L6 - covered")
  SXEL7("L7 - covered")
  SXEL8("L8 - covered")
  SXEL9("L9 - covered")

  local x = sxe.log_level
  sxe.log_level = 2
  local y = sxe.log_level
  SXEL1("old log level: " .. x)
  sxe.log_level = x

  is(test.bool, false, "test.bool==false");
  test.bool = true -- text-sxe-lua.c tests this

  is(test.int, 99, "test.int==99");
  test.int = 10  -- test-sxe-lua.c tests this

  is(test.uint, 88, "test.uint==88");
  test.uint = 128 -- test-sxe-lua.c tests this

  is(test.long, 77, "test.long==77");
  test.long = 42 -- text-sxe-lua.c tests this

  is(test.ulong, 66, "test.ulong==66");
  test.ulong = -2 -- text-sxe-lua.c tests this

  is(test.str, "Goodbye, cruel world", "test.str==Goodbye, cruel world");
  test.str = "Hello, world" -- text-sxe-lua.c tests this

  -- Failure cases
  ok1(not false) -- true!
  ok(1, "Is 1 true?") -- also true
  is(false, pcall(sxe_new_tcp), "sxe_new_tcp throws an error with no args")
  is(false, pcall(sxe_new_tcp, "", 0, "not a function"), "sxe_new_tcp throws error with bad conn arg")
  is(false, pcall(sxe_new_tcp, "", 0, nil, "not a function"), "sxe_new_tcp throws error with bad read arg")
  is(false, pcall(sxe_new_tcp, "", 0, nil, nil, "not a function"), "sxe_new_tcp throws error with bad close arg")
  is(false, pcall(sxe_write, ""), "sxe_write returns error on non-SXE object")
  diag("Just for coverage...")
  pass("This should pass")

  -- Pools
  local state = {[0]="FREE";"S1","S2","S3"}
  for i, v in pairs(state) do state[v] = i end
  local pool = sxe_pool_new("name", 10, 1 + #state)
  is(sxe_pool_get_name(pool), "name", "pool is named 'name'")
  is(sxe_pool_get_number_in_state(pool, state.FREE), 10, "10 free pool items")
  is(sxe_pool_get_oldest_element_index(pool, state.FREE), 9, "oldest element is 9")
  is(sxe_pool_index_to_state(pool, 9), state.FREE, "element 9's state is free")
  is(sxe_pool_get_oldest_element_index(pool, state.S1), nil, "no elements in state S1")
  sxe_pool_check_timeouts(pool)
  sxe_pool_touch_indexed_element(pool, 0)
  is(type(sxe_pool_get_indexed_element(pool, 0)), type({}), "get_indexed_element() returns a table")
  is(sxe_pool_set_oldest_element_state(pool, state.S1, state.S2), nil, "set_oldest_element_state fails on invalid state")
  is(sxe_pool_set_oldest_element_state(pool, state.FREE, state.S1), 9, "set_oldest_element_state returns affected element")
  sxe_pool_set_indexed_element_state(pool, 0, state.FREE, state.S1)
  -- NOT IMPLEMENTED: timed pools
  -- assert(sxe_pool_get_oldest_element_time(pool, state.S1) ~= nil)

  -- First guy: register 2: 1 listener, and 1 connection.
  local port
  sxe_register(2, function ()
    local l
    l = sxe_new_tcp("127.0.0.1", 0, nil, function (sxe)

      -- Coverage bumber.
      is(sxe.buf:find("Frankenstein", -100), nil, "sxe.buf:find() returns nil on failure")

      if sxe.buf:find("\r\n", -2) then
        local isexit = sxe.buf:ifind("eXIT\r\n", 1)
        sxe:write("IGOT:")
        sxe:write(sxe.buf)
        sxe:write("\r\n")
        sxe:buf_clear() -- not needed, but bumps coverage
        sxe:close()

        if isexit then l:close() end
      end
    end)
    l:listen()
    l.hello = "world"
    is(l.world, nil)
    port = l.local_port
    SXEL5(l.id .. ": listening on " .. l.local_addr .. " port " .. port)
  end)

  -- Test guy: register 2 clients
  sxe_register(2, function ()
    local c = sxe_new_tcp("0.0.0.0", 0,
      function (sxe)
        SXEL9("port: " .. sxe.peer_addr .. ":" .. sxe.peer_port)
        is(pcall(sxe_write, sxe, {}), false, "sxe_write cannot write a table")
        sxe:write("abcde\r\n")
      end,
      function (sxe)
        if sxe.buf:find("\r\n", sxe.buf_used - 2) then
          is(sxe.buf:find("IGOT:abcde"), 1, "sxe.buf contains IGOT:abcde")
        end
      end,
      function (sxe)
        local c2 = sxe_new_tcp("0.0.0.0", 0)
        c2.onconnect = function (sxe)
          sxe:write("exit\r\n")
        end
        c2.onread = function (sxe)
          if sxe.buf:find("IGOT:exit\r\n") then
            sxe:close()
          end
        end
        c2.onclose = function () end
        c2.onclose = function () do end end
        c2:connect("127.0.0.1", port)
      end)
    c:connect("127.0.0.1", port)

    is(pcall(sxe_spawn_kill, c, 10), false, "sxe_spawn_kill fails on non-spawned SXE")
  end)

  -- Run 'date' for a while
  sxe_register(2, function ()
    if sxe_spawn == nil then
        pass("spawn not implemented on windows")
        pass("spawn not implemented on windows")
        pass("spawn not implemented on windows")
    else
        is(pcall(sxe_spawn, "", "", "", "not function"), false, "Hello?")
        is(pcall(sxe_spawn, "", "", "", nil, "not function"), false, "Hello")
        is(pcall(sxe_spawn, "", "", "", nil, nil, "not function"), false, "Hello")
        local p = sxe_spawn("perl", "-le", "while(1){print scalar localtime;sleep 1}", nil, function (sxe) sxe:buf_clear() end, nil)

        local cd = 3
        local t
        t = sxe_timer_new(function ()
          cd = cd - 1
          if cd <= 0 then
            p:kill(15)
            sxe_timer_stop(t)
          end
        end, 0, 1)
        sxe_timer_start(t)
    end
  end)
end
-- vim: set ft=lua sw=2 sts=2 ts=8 expandtab list listchars=tab\:^.,trail\:@:
