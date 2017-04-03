 
def stencil(x, y, w, h):
	for s_y in [y-1, y, y+1]:
		for s_x in [x-1, x, x+1]:
			if s_x == -1:
				print "ghost left (0, %s)" % (s_y % h)
			elif s_x == w:
				print "ghost right (0, %s)" % (s_y % h)
			else:
				print "field (%s, %s)" % (((s_x + w) % w), ((s_y + h) % h))
	print "--\n"
				
def run(w = None, h = None):	
	if w is None or w < 2:
		w = 5
	if h is None or h < 1:
		h = 4
		
	for y in range(h):
		for x in range(w):
			stencil(x,y,w,h)
