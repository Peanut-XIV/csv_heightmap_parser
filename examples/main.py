from enum import Enum
from dataclasses import dataclass
from pathlib import Path
import os
import sys


class EOL(Enum):
    AUTO = 0
    UNIX = 1
    DOS = 2


@dataclass
class Field:
    min_size: int
    max_size: int

    @staticmethod
    def validate_int(
        val: int, low_bound: int | float, high_bound: int | float, name: str
    ) -> bool:
        """
        when validation fails, returns True and prints a message,
        otherwise returns False
        """
        invalid = False
        if val < low_bound:
            print(f"{name} must be greater than or equal to {low_bound}")
            invalid = True

        if val > high_bound:
            print(f"{name} must be less than or equal to {high_bound}")
            invalid = True
        return invalid

    def validate(self, mini, maxi, name) -> bool:
        """
        when validation fails, returns True and prints a message,
        otherwise returns False
        """
        invalid = False

        if self.validate_int(
            self.min_size, mini, maxi, f"{name} minimum size in bytes"
        ):
            invalid = True

        if self.validate_int(
            self.min_size, mini, maxi, f"{name} minimum size in bytes"
        ):
            invalid = True

        if self.min_size > self.max_size:
            print(f"{name} minimum size must be smaller than the maximum size")
            invalid = True

        return invalid


@dataclass
class c_parser_configuration:
    input_field: Field
    output_field: Field
    tile_width: int
    tile_height: int
    input_path: str
    output_path: str
    end_of_line: EOL = EOL.AUTO

    @staticmethod
    def validate_int(
        val: int, low_bound: int | float, high_bound: int | float, name: str
    ) -> bool:
        """
        when validation fails, returns True and prints a message,
        otherwise returns False
        """
        invalid = False
        if val < low_bound:
            print(f"{name} must be greater than or equal to {low_bound}")
            invalid = True

        if val > high_bound:
            print(f"{name} must be less than or equal to {high_bound}")
            invalid = True
        return invalid

    @staticmethod
    def validate_input_path(in_path: str | Path) -> bool:
        """
        when validation fails, returns True and prints a message,
        otherwise returns False
        """
        in_path = Path(in_path)
        if not in_path.exists():
            print("Input path must exist")
            return True
        if not in_path.is_file():
            print("Input path must point to a file")
            return True
        return False

    @staticmethod
    def validate_output_path(in_path) -> bool:
        """
        when validation fails, returns True and prints a message,
        otherwise returns False
        """
        in_path = Path(in_path)
        if not in_path.exists():
            if not in_path.parent.exists():
                print("output path direct parent does not exist")
                return True
            if not in_path.parent.is_dir():
                print("output path direct parent must be a directory")
                return True
        else:
            if not in_path.is_dir():
                print("output path exists, then it must point to a directory")
                return True
        return False

    @staticmethod
    def validate_eol(eol) -> bool:
        """
        when validation fails, returns True and prints a message,
        otherwise returns False
        """
        if not isinstance(eol, EOL):
            print("end_of_line has an invalid value")
            return True
        return False

    def validate(self) -> bool:
        MAX_SHORT: int = 2**15 - 1
        MAX_INT: int = 2**31 - 1

        checks = [
            self.input_field.validate(1, MAX_SHORT, "input field"),
            self.output_field.validate(1, MAX_SHORT, "output field"),
            self.validate_int(self.tile_width, 0, MAX_INT, "tile width"),
            self.validate_int(self.tile_height, 0, MAX_INT, "tile height"),
            self.validate_input_path(self.input_path),
            self.validate_output_path(self.output_path),
            self.validate_eol(self.end_of_line),
        ]

        invalid = any(checks)
        return invalid

    def eol_flag_char(self) -> str:
        flag_char = None
        if self.end_of_line == EOL.UNIX:
            flag_char = "u"
        elif self.end_of_line == EOL.DOS:
            flag_char = "d"
        else:
            flag_char = "a"
        return flag_char

    def generate_config_file(self, config_path, overwrite=False):
        config_path = Path(config_path)

        if self.validate():
            print("Invalid configuration.")
            return True

        if config_path.exists():
            print("destination file already exists")
            if not overwrite:
                return True
        else:
            if not config_path.parent.exists():
                print("destination file direct parent does not exist")
                return True
            if not config_path.parent.is_dir():
                print("destination file direct parent is not a directory")
                return True

        lines = [
            f"min_field_size = {self.input_field.min_size}\n",
            f"max_field_size = {self.input_field.max_size}\n",
            f"output_field_size = {self.output_field.max_size}\n",
            f"eol_flag = {self.eol_flag_char()}\n",
            f"tile_width = {self.tile_width}\n",
            f"tile_height = {self.tile_height}\n",
            f'source = "{str(self.input_path)}"\n',
            f'dest = "{str(self.output_path)}"\n',
        ]

        try:
            file = open(config_path, "w", encoding="UTF-8")
            file.writelines(lines)
            file.close()
        except OSError:
            print("failed to open file for writing")
            return True
        return False


def run_program(exec_path: str, config_path: str):
    try:
        os.execl(exec_path, exec_path, config_path)
    except OSError:
        print(f"could not execute the process {exec_path}")
        return True


if __name__ == "__main__":
    # set the source file path
    ipath = "/Users/louis/Programming/internship/inputs/3D/ODP_208_1262B_22H_3_65-66cm_967P_90A_3D.csv"
    # set the destination directory
    opath = "/Users/louis/Programming/internship/output/benchmark/ODP_BIS/"
    # set the path of the configuration file used with the parser
    # it can be a temporary file
    conf_path = "/Users/louis/Programming/internship/inputs/3D/ODP_BIS.toml"
    # set the path of the parser executable (can be put anywhere in the filesystem)
    exec_path = "/Users/louis/Programming/internship/c_parser/build/Release/parser"

    # set the bounds of size of the fields containing numeric values
    # inside the source csv
    ifld = Field(min_size=5, max_size=8)
    ofld = Field(min_size=5, max_size=8)

    # initialize the config object
    config = c_parser_configuration(
        input_field=ifld,
        output_field=ofld,
        tile_width=1000,
        tile_height=1000,
        input_path=ipath,
        output_path=opath,
        end_of_line=EOL.AUTO,
    )

    # Validate the configuration and creates a config file at the given path
    if config.generate_config_file(conf_path, overwrite=True):
        print("failed to generate config file")
        sys.exit(0)

    # Run the program. May fail if the executable does not have the correct permission
    if run_program(exec_path, conf_path):
        print("failed to run the csv parser")
        sys.exit(0)
