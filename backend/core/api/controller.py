from flask import Blueprint, request, url_for
# from core.api.data_schema import 

from core.api.api_response_wrapper import APIResponse, AddData, GetData

api = Blueprint("api", __name__)
csv_file_path = "./core/csv_data/"

"""
Add data to the CSV file.
It validates the input data using the schema.
If the data is valid, it appends it to the CSV file and returns a success response.
If the data is invalid or could not be added to the CSV file, it returns an error response.
"""

#First API Endpoint for ChilledWaterMonitor
@api.route("/chilled-watermonitor", methods=["POST"])
def add_chilled_watermonitor_data():
    data = request.get_json()
    print(data)
    return 