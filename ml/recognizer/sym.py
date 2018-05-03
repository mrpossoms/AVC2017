import sys

class Li():
    def __init__(self, description):
        self.desc = description

        self.present = {}

        for arg in sys.argv:
            parts = arg.split('=')
            name = parts[0].strip()
            value = True

            if len(parts) > 1:
                value = parts[1].strip()
            self.present[name] = value

    def __getitem__(self, item):
        if item in self.present:
            return self.present[item]
        else:
            return False

    def optional(self, argument, description):
        return self

    def required(self, argument, description):
        if argument not in self.present:
            print('Error: Missing required ' + argument + ' ' + description)
            exit(-1)

        return self