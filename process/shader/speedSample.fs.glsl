in float speed;
out vec4 result;

void main() {
	result = vec4(speed, 0.0, 0.0, step(speed, 0.0));
}
