import matplotlib.pyplot as plt
import ast
import sys

def draw_points(X, Y, marker_shape = '.'):
    if marker_shape == 'o':
        plt.scatter(X, Y, marker=marker_shape, c = 'white', edgecolors='black', s = 150, linewidths=2)
    else:
        plt.scatter(X, Y, marker=marker_shape, color='black')

def draw_arrows(E):
    for e in E:
        plt.annotate("", xy=(e[1][0], e[1][1]), xytext=(e[0][0], e[0][1]), arrowprops=dict(arrowstyle="->"),size=24)

def setup():
    N = 10
    for i in range(N):
        X = []
        Y = []
        for j in range(N):
            X.append(i)
            Y.append(j)
        draw_points(X, Y)

def main():
    setup()
    if len(sys.argv) != 3:
        print("Usage:")
        print("       python draw.py raw_data output.png")
        exit(1)
    try:
        parsed_data = ast.literal_eval(sys.argv[1])
    except:
        print(sys.argv)
        exit(1)

    draw_arrows(parsed_data)
    plt.savefig(sys.argv[2])

main()