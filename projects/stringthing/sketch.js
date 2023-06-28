let canvas_dimensions;

function setup()
{
	createCanvas(windowWidth, windowHeight);
}

function draw()
{
	background(255);
	line(mouseX, 0, mouseX, height);
	line(0, mouseY, width, mouseY);
}

function mouseMoved()
{
	Bela.data.sendBuffer(0, 'float', [mouseX / width, 1 - mouseY / height]);
}
