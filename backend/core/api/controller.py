from flask import Blueprint, request, url_for
from core.api.data_schema import battery_schema, temperature_schema, speed_schema, engine_schema, solar_schema, chassis_schema, validate_data, vesc_duty_cycle_schema, vesc_stat1_schema, vesc_stat2_schema, vesc_stat3_schema, vesc_stat4_schema, vesc_stat5_schema, bms_power_schema, bms_temp_state_schema
from core.api.db_convert import append_data, get_data_all, get_data

from core.api.api_response import APIResponse, AddData, GetData

api = Blueprint("api", __name__)
csv_file_path = "./core/csv_data/"

"""
Add data to the CSV file.
It validates the input data using the schema.
If the data is valid, it appends it to the CSV file and returns a success response.
If the data is invalid or could not be added to the CSV file, it returns an error response.
"""