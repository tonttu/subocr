#!/usr/bin/ruby
# encoding: utf-8

def utf8ize data
  data.valid_encoding? ? data : data.encode('UTF-8', 'ISO-8859-1')
end

def dos? data
  data.scan(/\r\n/).size > 0
end

def latin1 data
  data.gsub('š', 's').encode('ISO-8859-1')
end

if ARGV.size != 7
  $stderr.puts "Usage: combine.rb basename <point 1> <point 2>"
  $stderr.puts "       point: <delay ms> <offset min> <offset sec>"
  exit 0
end

diff1 = ARGV[1].to_i / 1000.0
diff2 = ARGV[4].to_i / 1000.0

episode = 0
basename = ARGV[0].sub(/E(\d\d)/) do |b|
  episode = $1.to_i
  'E%02d'
end

s1 = ARGV[2].to_i * 60 + ARGV[3].to_f
s2 = ARGV[5].to_i * 60 + ARGV[6].to_f

time = s2 - s1
diff = diff2 - diff1

diff_per_second = diff / (time-diff)

$shift = (diff1 - s1 * diff_per_second) * 1000.0

$factor = 1.0 + diff_per_second

def tt t
  t = t.to_i
  t = 1000 if t < 1000 # some players crash without this
  c = t % 60000
  t /= 60000
  b = t % 60
  "%02d:%02d:%06.3f" % [t / 60, b, c*0.001]
end

def apply t
  t * $factor + $shift
end

data = $stdin.read
newline = dos?(data) ? "\r\n" : "\n"
data = utf8ize data.strip.gsub("\r", "")

titles = []
start = 0
File.read('titles').strip.split(/[\r\n]+/).map do |line|
  title, len = line.split
  title = title.to_i
  len = len.to_f * 1000
  break if len < 600000
  titles << {:title => title, :len => len, :start => start, :stop => start + len, :subnum => 0, :start2 => start*$factor}
  start += len
end

output = []

data.split(/\n\s*\n(\n\s)*/).each_with_index do |s, i|
  if s =~ /^\d+\n(\d*):(\d*):([\d,.]*)\s*-->\s*(\d*):(\d*):([\d,.]*)/
    begin
      start = $1.to_i*3600000 + $2.to_i * 60000 + ($3.tr(',', '.').to_f * 1000).to_i
      stop = $4.to_i*3600000 + $5.to_i * 60000 + ($6.tr(',', '.').to_f * 1000).to_i

      start2 = apply(start)
      stop2 = apply(stop)

      txt = File.read(s.split("\n")[2..-1].join).split(/[\r\n]+/).join(newline)

      next if txt.empty?
      output << [start, stop, start2, stop2, txt]
    rescue Kernel => e
      $stderr.puts "Error: '#{e}' at sub #{i}: '#{s}'"
      $stderr.puts e.backtrace
    end
  else
    $stderr.puts "Couldn't parse at sub #{i}: '#{s}'"
  end
end

margin = 30000
titles.each_with_index do |t, i|
  t[:file] = File.open(basename % (i + episode), "wb")
end

output.each do |start, stop, start2, stop2, txt|
  titles.each do |t|
    if start - margin < t[:stop] && stop + margin > t[:start] && stop2-t[:start2] > 1000
      ts = "%s --> %s" % [tt(start2-t[:start2]), tt(stop2-t[:start2])]
      t[:file].print "#{t[:subnum] += 1}#{newline}"
      t[:file].print "#{ts.gsub('.', ',')}#{newline}"
      t[:file].print "#{latin1 txt}#{newline*2}"
    end
  end
end
