#!/usr/bin/ruby

title = 1
loop do
  output = `dvdxchap -t #{title} . 2>/dev/null`
  break if output.empty?
  h, m, s = output.scan(/CHAPTER\d+=(\d+):(\d+):([\d.]+)/).last
  secs = h.to_i * 60*60 + m.to_i * 60 + s.to_f
  puts "#{title}\t#{secs}"
  title += 1
end
